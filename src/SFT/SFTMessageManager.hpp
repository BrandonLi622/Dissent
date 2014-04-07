#ifndef SFTMESSAGEMANAGER_HPP
#define SFTMESSAGEMANAGER_HPP

#include <QVariant>

namespace Dissent {
namespace SFT {
class SFTMessageManager : public QObject
{
    Q_OBJECT
    Q_ENUMS(MessageTypes)

public:
    //-Notify client of view change   (broadcast to clients)       <== ALL
    //-Vote on the view change        (broadcast to servers?)      <== ALL

    //TODO: I suppose I can make this class static

    enum MessageTypes {
      ViewChangeVote,
      ViewChangeNotification
    };

    SFTMessageManager();

    QByteArray genViewChangeProposal(QVariantList viewNums);
    QByteArray genClientViewChangeNotification(quint32 newViewNum);
    QByteArray encryptionKey(quint64 serverID, quint64 clientID, quint32 keyLength);

};
}
}

#endif // SFTMESSAGEMANAGER_HPP
