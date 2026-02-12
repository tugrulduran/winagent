#pragma once

#include <QTcpServer>
#include <QSslSocket>

class DashboardServer : public QTcpServer {
    Q_OBJECT

public:
    explicit DashboardServer(QObject *parent = nullptr);

public slots:
    void start();

    void stop();

signals:
    void finished();

    void started();

    void stopped();

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onEncrypted();

    void onReadyRead();
};
