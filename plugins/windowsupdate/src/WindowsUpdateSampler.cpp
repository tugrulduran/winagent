#include "WindowsUpdateSampler.h"

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <wuapi.h>
#endif

#include <sstream>

namespace windowsupdate {

void WindowsUpdateSampler::init(const QJsonObject& cfg) {
    Q_UNUSED(cfg);
}

QString WindowsUpdateSampler::bstrToQString(const wchar_t* bstr) {
    return bstr ? QString::fromWCharArray(bstr) : QString();
}

static QString hrToQString(HRESULT hr) {
    return QString("0x%1").arg(static_cast<unsigned long>(hr), 8, 16, QChar('0')).toUpper();
}

static QString normalizeGuid(QString s) {
    s = s.trimmed().toUpper();
    if (s.startsWith('{') && s.endsWith('}') && s.size() > 2) {
        s = s.mid(1, s.size() - 2);
    }
    return s;
}

// WSUS classification GUIDs (also used as category IDs in WUA)
// Source: Microsoft Learn "WSUS Classification GUIDs"
static const QString kGuidCriticalUpdates = QStringLiteral("E6CF1350-C01B-414D-A61F-263D14D133B4");
static const QString kGuidSecurityUpdates = QStringLiteral("0FA1201D-4330-4FA8-8AE9-B877473B6441");

bool WindowsUpdateSampler::scan(std::vector<UpdateInfo>& outUpdates, QString& err) {
    outUpdates.clear();
    err.clear();

#ifndef _WIN32
    err = "WindowsUpdateSampler is supported on Windows only";
    return false;
#else
    // WUA is a COM API.
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninit = (hr == S_OK || hr == S_FALSE);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        err = "CoInitializeEx failed: " + hrToQString(hr);
        return false;
    }

    IUpdateSession* session = nullptr;
    IUpdateSearcher* searcher = nullptr;
    ISearchResult* result = nullptr;
    IUpdateCollection* updates = nullptr;

    auto cleanup = [&]() {
        if (updates) updates->Release();
        if (result) result->Release();
        if (searcher) searcher->Release();
        if (session) session->Release();
        if (shouldUninit) CoUninitialize();
    };

    hr = CoCreateInstance(CLSID_UpdateSession, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&session));
    if (FAILED(hr) || !session) {
        err = "CoCreateInstance(UpdateSession) failed: " + hrToQString(hr);
        cleanup();
        return false;
    }

    hr = session->CreateUpdateSearcher(&searcher);
    if (FAILED(hr) || !searcher) {
        err = "CreateUpdateSearcher failed: " + hrToQString(hr);
        cleanup();
        return false;
    }

    // Criteria: not installed and not hidden.
    BSTR criteria = SysAllocString(L"IsInstalled=0 and IsHidden=0");
    if (!criteria) {
        err = "SysAllocString(criteria) failed";
        cleanup();
        return false;
    }

    hr = searcher->Search(criteria, &result);
    SysFreeString(criteria);

    if (FAILED(hr) || !result) {
        err = "Update search failed: " + hrToQString(hr);
        cleanup();
        return false;
    }

    hr = result->get_Updates(&updates);
    if (FAILED(hr) || !updates) {
        err = "get_Updates failed: " + hrToQString(hr);
        cleanup();
        return false;
    }

    LONG count = 0;
    updates->get_Count(&count);
    if (count <= 0) {
        cleanup();
        return true;
    }

    outUpdates.reserve(static_cast<size_t>(count));

    for (LONG i = 0; i < count; ++i) {
        IUpdate* upd = nullptr;
        hr = updates->get_Item(i, &upd);
        if (FAILED(hr) || !upd) continue;

        // Identity
        IUpdateIdentity* ident = nullptr;
        BSTR updateIdB = nullptr;
        LONG rev = 0;

        hr = upd->get_Identity(&ident);
        if (SUCCEEDED(hr) && ident) {
            ident->get_UpdateID(&updateIdB);
            ident->get_RevisionNumber(&rev);
        }

        // Title
        BSTR titleB = nullptr;
        upd->get_Title(&titleB);

        // Type (software / driver)
        UpdateType upType = utSoftware;
        upd->get_Type(&upType);

        // Optional-ness (BrowseOnly / AutoSelectOnWebSites)
        VARIANT_BOOL autoSelect = VARIANT_TRUE;
        upd->get_AutoSelectOnWebSites(&autoSelect);

        bool browseOnly = false;
        {
            IUpdate3* upd3 = nullptr;
            if (SUCCEEDED(upd->QueryInterface(IID_PPV_ARGS(&upd3))) && upd3) {
                VARIANT_BOOL b = VARIANT_FALSE;
                if (SUCCEEDED(upd3->get_BrowseOnly(&b))) {
                    browseOnly = (b == VARIANT_TRUE);
                }
                upd3->Release();
            }
        }

        // Classifications (categories) for importance
        bool hasCritical = false;
        bool hasSecurity = false;
        {
            ICategoryCollection* cats = nullptr;
            if (SUCCEEDED(upd->get_Categories(&cats)) && cats) {
                LONG ccount = 0;
                cats->get_Count(&ccount);
                for (LONG ci = 0; ci < ccount; ++ci) {
                    ICategory* cat = nullptr;
                    if (FAILED(cats->get_Item(ci, &cat)) || !cat) continue;
                    BSTR catIdB = nullptr;
                    if (SUCCEEDED(cat->get_CategoryID(&catIdB)) && catIdB) {
                        const QString catId = normalizeGuid(bstrToQString(catIdB));
                        if (catId == kGuidCriticalUpdates) hasCritical = true;
                        if (catId == kGuidSecurityUpdates) hasSecurity = true;
                        SysFreeString(catIdB);
                    }
                    cat->Release();
                    if (hasCritical || hasSecurity) break;
                }
                cats->Release();
            }
        }

        UpdateInfo info;
        const QString updateId = bstrToQString(updateIdB);
        if (!updateId.isEmpty()) {
            info.id = QString("%1:%2").arg(updateId).arg(static_cast<int>(rev));
        } else {
            // fallback: index based id
            info.id = QString("idx:%1").arg(i);
        }
        info.title = bstrToQString(titleB);

        // Type string
        if (upType == utDriver) info.type = QStringLiteral("driver");
        else if (upType == utSoftware) info.type = QStringLiteral("software");
        else info.type = QStringLiteral("other");

        // Importance heuristic:
        // - BrowseOnly => optional (typically shown under Optional updates)
        // - Critical/Security classifications => critical
        // - Not auto-selected => optional
        // - Otherwise => important
        if (browseOnly) {
            info.importance = QStringLiteral("optional");
        } else if (hasCritical || hasSecurity) {
            info.importance = QStringLiteral("critical");
        } else if (autoSelect == VARIANT_FALSE) {
            info.importance = QStringLiteral("optional");
        } else {
            info.importance = QStringLiteral("important");
        }

        // Clean up BSTRs and COM
        if (titleB) SysFreeString(titleB);
        if (updateIdB) SysFreeString(updateIdB);
        if (ident) ident->Release();
        upd->Release();

        outUpdates.push_back(std::move(info));
    }

    cleanup();
    return true;
#endif
}

} // namespace windowsupdate
