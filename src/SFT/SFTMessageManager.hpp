#ifndef SFTMESSAGEMANAGER_HPP
#define SFTMESSAGEMANAGER_HPP

#include <QVariant>

namespace Dissent {
namespace SFT {
class SFTMessageManager
{
public:
    //-Notify client of view change   (broadcast to clients)       <== ALL
    //-Propose view change            (broadcast to servers)       <== PROPOSER (ditch this, just have everyone cast n votes)
    //-Vote on the view change        (broadcast to servers?)      <== ALL
    //-Announce view change           (broadcast to servers)       <== PROPOSER

    //TODO: I suppose I can make this class static

    SFTMessageManager();

    QByteArray genViewChangeProposal(QVariantList viewNums);
    QByteArray genClientViewChangeNotification(quint32 newViewNum);
};
}
}

#endif // SFTMESSAGEMANAGER_HPP
