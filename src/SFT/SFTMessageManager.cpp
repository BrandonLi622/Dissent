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


QByteArray SFTMessageManager::encryptionKey(quint64 serverID, quint64 clientID, quint32 keyLength)
{
    QByteArray key;
    srand(serverID * 100 + clientID);
    for (int i = 0; i < numClients; i++)
    {
        for (int j = 0; j < keyLength; j++)
        {
            char num = char(32 + rand() % 96); //Just for testing purposes make them all printable characters
            key.append(num); // Pick a random character
            qDebug() << "chosen number " << num;

        }
    }

    return key;
}

//Utility
QByteArray xorBytes(QByteArray a1, QByteArray a2)
{
    //Assuming strings are the same length
    QByteArray xored;

    if (a1.length() != a2.length())
    {
        qDebug() << "Warning: xoring 2 bytearrays of different lengths";
    }

    for (int i = 0; i < a1.length(); i++)
    {
        xored.append(a1.at(i) ^ a2.at(i));
    }
    return xored;
}



}
}
