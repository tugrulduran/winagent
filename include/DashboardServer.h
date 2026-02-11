#pragma once

#include <QTcpServer>
#include <QSslSocket>

class DashboardServer : public QTcpServer
{
    Q_OBJECT

public:
    explicit DashboardServer(QObject* parent = nullptr);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onEncrypted();
    void onReadyRead();
};
