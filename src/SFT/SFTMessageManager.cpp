#include "SFTMessageManager.hpp"

namespace Dissent{
namespace SFT {
SFTMessageManager::SFTMessageManager()
{
    //genClientViewChangeNotification(100);
    QVariantList list = QVariantList();
    list.append(100);
    //genViewChangeProposal(list);
}

QByteArray genViewChangeProposal(QVariantList viewNums)
{
    return QVariant(viewNums).toByteArray();
}

QByteArray genClientViewChangeNotification(quint32 newViewNum)
{
    return QVariant(newViewNum).toByteArray();
}


}
}
