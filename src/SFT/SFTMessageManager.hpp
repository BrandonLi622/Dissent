#ifndef SFTMESSAGEMANAGER_HPP
#define SFTMESSAGEMANAGER_HPP

#include <QVariant>
#include "Connections/Id.hpp"
#include "SFT/SFTNullRound.hpp"

namespace Dissent {
namespace SFT {
class SFTNullRound;

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
    //ServerCipher ("Cipher", QByteArray)
    //Decrypted ("Decrypted", QByteArray) and ("ViewNum", int)
    //ClientAttendance ("ClientList", QList<QVariant>)

    enum MessageTypes {
      ViewChangeVote,
      ViewChangeNotification,
      ClientMessage,
      ServerCipher,
      Decrypted,
      ClientAttendance
    };

    SFTMessageManager(SFT::SFTNullRound *round);

    //Generate maps for me to send
    QVariantMap genViewChangeProposal(qint32 viewNum);
    QVariantMap genClientViewChangeNotification(qint32 viewNum);
    QVariantMap genDecryption(qint32 viewNum);

    //Working with message logic
    QByteArray encryptionKey(qint32 serverID, qint32 clientID, qint32 keyLength, qint32 numClients);
    QByteArray xorBytes(QByteArray a1, QByteArray a2);
    QByteArray createCipher();
    QByteArray getDecrypted();

    //Receiving messages
    void switchServerMessage(QVariantMap map, const Connections::Id &from);
    void switchClientMessage(QVariantMap map, const Connections::Id &from);

    //Phases
    void startRound();
    void startClientAttendance();
    void startCipherExchange();
    void startViewVoting();

    //Client functions
    QByteArray encryptClientMessage(int clientID, QByteArray data); //Above were mostly server functions
    QByteArray insertInSlot(QString msg);
    //TODO: Need something for clients to send messages

private:
    void processClientMessage(QVariantMap map, const Connections::Id &from);
    void processClientList(QVariantMap map, const Connections::Id &from);
    void processViewChangeProposal(QVariantMap map, const Connections::Id &from);
    void processCipher(QVariantMap map, const Connections::Id &from);

    qint32 roundPhase;
    QHash<qint32, QByteArray> *receivedClientMsgs;
    QHash<qint32, QByteArray> *receivedServerCiphers;
    QHash<qint32, QVariantMap> *viewChangeProposals;
    QList<qint32> *respondedServers;
    QList<qint32> *respondedClients;
    QList<qint32> *totalRoundClients;
    SFT::SFTNullRound *round;

    int keyLength = 3;


};
}
}

#endif // SFTMESSAGEMANAGER_HPP
