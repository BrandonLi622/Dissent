#include "SFTMessageManager.hpp"

namespace Dissent{
namespace SFT {
SFTMessageManager::SFTMessageManager()
{
    ;
}

QByteArray SFTMessageManager::genViewChangeProposal(QVariantList viewNums)
{
    return QVariant(viewNums).toByteArray();
}

QByteArray SFTMessageManager::genClientViewChangeNotification(quint32 newViewNum)
{
    return QVariant(newViewNum).toByteArray();
}


}
}
