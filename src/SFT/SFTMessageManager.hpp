#ifndef SFTMESSAGEMANAGER_HPP
#define SFTMESSAGEMANAGER_HPP

#include <QVariant>
#include <QTimer>
#include <QDateTime>
#include "Connections/Id.hpp"
#include "SFT/SFTViewManager.hpp"
#include "SFT/SFTNullRound.hpp"


namespace Dissent {
namespace SFT {
class SFTNullRound;
class SFTViewManager;

class SFTMessageManager : public QObject
{
    Q_OBJECT
    Q_ENUMS(MessageTypes)
    Q_ENUMS(RoundPhase)

public:
    //-Notify client of view change   (broadcast to clients)       <== ALL
    //-Vote on the view change        (broadcast to servers?)      <== ALL

    //All messages should have key "Type"
    //ViewChangeVote: ("ViewNum", int)
    //ViewChangeNotification: ("ViewNum", int)
    //ClientMessage ("ViewNum", int) and ("Message", QByteArray)
    //ForwardedClientMessage("ViewNum", int) and ("Message", QByteArray) and ("ClientId", int)
    //ServerCipher ("Cipher", QByteArray)
    //Decrypted ("Decrypted", QByteArray) and ("ViewNum", int)
    //ClientAttendance ("ClientList", QList<QVariant>)

    const static int keyLength = 3;

    enum MessageTypes {
      ViewChangeVote,
      ViewChangeNotification,
      ClientMessage,
      ForwardedClientMessage,
      ServerCipher,
      Decrypted,
      ClientAttendance,
      RequestInfo,
      RequestInput
    };

    SFTMessageManager(Connections::Id localId,
                      const Identity::Roster &clients,
                      const Identity::Roster &servers,
                      QList<Connections::Id> connections,
                      bool isServer,
                      SFT::SFTViewManager *sftManager);

    //Generate maps for me to send
    QVariantMap genViewChangeProposal(int viewNum);
    QVariantMap genClientViewChangeNotification(int viewNum);
    QVariantMap genDecryption(int viewNum);
    QVariantMap genClientMessage(int viewNum, QByteArray data);
    QVariantMap genRequest(int type);
    QVariantMap genInputRequest();

    //Working with message logic
    QByteArray encryptionKey(int serverID, int clientID, int keyLength, int numClients);
    QByteArray xorBytes(QByteArray a1, QByteArray a2);
    QByteArray createCipher();
    QByteArray getDecrypted();

    //Utilities
    QList<QVariant> getReceivedClients();

    //Sending messages
    void sendViewChangeProposal(int viewNum);
    void sendRequest(int type);
    void sendInputRequest();
    void forwardClientMessage(QVariantMap msg, const Connections::Id &from);

    //Receiving messages
    void switchServerMessage(QVariantMap map, const Connections::Id &from);
    void switchClientMessage(QVariantMap map, const Connections::Id &from);

    //Client functions
    void clientRoundMessageSend(QString msg);

public slots:
    //Server Phases
    void startServerRound();
    void startClientAttendance();
    void startCipherExchange();
    void startViewVoting();


signals:
    void sendToSingleNode(Connections::Id peer, QVariantMap map);
    void broadcastToServers(QVariantMap map);
    void broadcastToEveryone(QVariantMap map);
    void broadcastToDownstreamClients(QVariantMap map);
    void pushDataOut(QByteArray data);
    void roundSuccessful();
    void messageExchangeEnded();


private:
    //Server functions
    void processClientMessage(QVariantMap map, const Connections::Id &from);
    //void processForwardedClientMessage(QVariantMap map, const Connections::Id &from);
    void processClientList(QVariantMap map, const Connections::Id &from);
    void processViewChangeProposal(QVariantMap map, const Connections::Id &from);
    void processCipher(QVariantMap map, const Connections::Id &from);
    void processInfoRequest(QVariantMap map, const Connections::Id &from);

    //Client functions
    QByteArray encryptClientMessage(int clientID, QByteArray data); //Above were mostly server functions
    QByteArray insertInSlot(QString msg);
    void processViewChangeNotification(QVariantMap map, const Connections::Id &from);
    void processDecrypted(QVariantMap map);
    void processInputRequest(QVariantMap map, const Connections::Id &from);

    int roundPhase;
    QHash<int, QByteArray> *receivedClientMsgs;
    QHash<int, QByteArray> *receivedServerCiphers;
    QHash<int, QVariantMap> *viewChangeProposals;


    QList<int> *respondedServers_Attendance;
    QList<int> *respondedServers_Cipher;
    QList<int> *respondedServers_ViewChange;


    QList<int> *respondedClients;
    //QList<int> *respondedForwardedClients;
    QList<int> *totalRoundClients;


    Connections::Id localId;
    Identity::Roster m_clients;
    Identity::Roster m_servers;
    bool isServer;
    QList<Connections::Id> m_connections;
    SFT::SFTViewManager *sftViewManager;

    QByteArray lastCipher; //So that the slowest person in a round can get the ciphertexts

    QTimer *timer;


    QDateTime roundStartTime;
    QDateTime viewChangeStartTime;

};
}
}

#endif // SFTMESSAGEMANAGER_HPP
