#include "SFTViewManager.hpp"

namespace Dissent{
namespace SFT {
SFTViewManager::SFTViewManager(const Identity::Roster &servers,
                               double quorumRatio) :
    m_servers(servers)
{
    this->downServers = new QVector<Connections::Id>();
    this->numServers = m_servers.Count();
    this->quorumRatio = quorumRatio;
    this->viewNum = 0;
    this->proposedViewNum = 0;
    this->currentView = new QVector<bool>();
    this->viewChangeProposals = new QHash<int, QVariantMap>();

    setNewView(this->viewNum);


    qDebug() << "SFTViewManager constructor IS WORKING" << *(this->currentView);
    //qDebug() << this->currentView;
}

int SFTViewManager::getCurrentViewNum()
{
    return this->viewNum;
}

int SFTViewManager::getProposedViewNum()
{
    return this->proposedViewNum;
}

void SFTViewManager::proposeNewView(int viewNum)
{
    this->proposedViewNum = viewNum;
}

bool SFTViewManager::inCurrentView(const Connections::Id &nodeId)
{
    int index = this->m_servers.GetIndex(nodeId);
    return (*(this->currentView))[index];
}

bool SFTViewManager::addFailedServer(const Connections::Id &nodeId)
{

    if (this->downServers->contains(nodeId))
    {
        qDebug() << "Server is already down";
        return false;
    }

    qDebug() << "Adding failed server: " << nodeId;
    this->downServers->append(nodeId);
    return this->inCurrentView(nodeId);
}

QVector<Connections::Id> SFTViewManager::getCurrentServers()
{
    //TODO: How long will this value persist?
    QVector<Connections::Id> connections = QVector<Connections::Id>();

    for (int i = 0; i < this->currentView->count(); i++)
    {
        if (this->currentView->at(i))
        {
            connections.append(this->m_servers.GetId(i));
        }
    }

    return connections;
}

bool SFTViewManager::isFirstInView(const Connections::Id &nodeId)
{
    return getCurrentServers().at(0) == nodeId;
}

int SFTViewManager::getNumLiveServers()
{
    return this->numServers - this->downServers->count();
}

int SFTViewManager::nextGoodView(int minViewNum)
{

    QVector<int> *badServers = new QVector<int>();
    for (int j = 0; j < this->downServers->count(); j++)
    {
        const Connections::Id downServer = downServers->at(j);
        int index = this->m_servers.GetIndex(downServer);

        qDebug() << "Downserver: " << downServer << index;
        badServers->append(index);
    }

    int nextViewNum = minViewNum + 1;

    QVector<bool> *viewMembership;

    while(true)
    {
        qDebug() << "Bad servers: " << *badServers;
        viewMembership = calcServerMembership(nextViewNum);

       bool viewGood = true;

       for (int i = 0; i < badServers->count(); i++)
       {
           int index = badServers->at(i);

           if (viewMembership->at(index))
           {

               viewGood = false;
           }
       }

       if (viewGood)
       {
           qDebug() << "Found the view " << nextViewNum << ":" << *viewMembership << "Bad servers: " << *badServers;

           qDebug() << "All votes" << *this->viewChangeProposals;
           return nextViewNum;
       }
       nextViewNum++;


       //TODO: Need to add this back in!!
       //delete(viewMembership);
    }
}



bool SFTViewManager::tooFewServers()
{
    return (this->numServers - this->downServers->count()) < 2.0 * this->numServers / 3.0;
}


int SFTViewManager::addViewChangeVote(int viewNum, bool vote, const Connections::Id &voter)
{
    int serverID = voter.GetInteger().GetInt32();
    QVariantMap countMap = viewChangeProposals->value(viewNum, QVariantMap()); //want it to default to 0

    int count = countMap.value("Count", 0).toInt();
    int rejections = countMap.value("Count", 0).toInt();
    QList<QVariant> voters = countMap.value("Voters", QList<QVariant>()).toList();

    //Don't allow double counting
    //TODO: This isn't quite right...
    if (voters.contains(serverID))
    {
        qDebug() << "Redundant voters " << voters;
        return -1;
    }
    qDebug() << "COUNT: " << (count + 1);

    voters.append(serverID);

    //Depending on the vote, then increment the correct amount
    vote ? count++ : rejections++;

    countMap.insert("Count", count);
    countMap.insert("Rejections", rejections);
    countMap.insert("Voters", voters);

    viewChangeProposals->insert(viewNum, countMap);

    //TODO: should move out this proportion somewhere else...
    if (count >= 2.0 * this->m_servers.Count() / 3.0 && viewNum > getCurrentViewNum())
    {
        qDebug() << "Before " << *(this->currentView);
        for (int i = 0; i < this->currentView->count(); i++)
        {
            qDebug() << this->m_servers.GetId(i);
        }

        qDebug() << "Changed view";
        setNewView(viewNum);
        qDebug() << "After " << *(this->currentView);

        return viewNum;
        //startRound(); //Need to discard everything and start a new round when the view changes
    }
    else if (rejections >= 1.0 * this->m_servers.Count())
    {
        //View change failed, so set back
        this->proposedViewNum = this->viewNum;
        this->viewChangeProposals->clear(); //TODO: Should this be cleared?
        return -2;
    }
    return -1; //Means view did not change
}

bool SFTViewManager::setNewView(int viewNum)
{
    delete this->currentView;
    this->viewNum = viewNum;
    this->currentView = calcServerMembership(viewNum);
    this->viewChangeProposals->clear(); //TODO: Should this be cleared?

    return true;
}

int SFTViewManager::getViewSize()
{
    int count = 0;
    for (int i = 0; i < this->currentView->count(); i++)
    {
        if (this->currentView->at(i))
        {
            count++;
        }
    }
    return count;
}

QVector<bool> *SFTViewManager::calcServerMembership(int viewNum)
{
    QVector<bool> *viewMembership = new QVector<bool>();
    srand(viewNum * 100); //TODO: is this good enough for variance?

    //everything is initially false
    for (int i = 0; i < this->numServers; i++)
    {
        viewMembership->push_back(false);
    }

    //TODO: same ratio for view change and for number of people in view?
    for (int memberCount = 0; memberCount < this->quorumRatio * this->numServers; )
    {
        //Select another member to add to the set
        int nextMember = rand() % this->numServers;
        if (!(*viewMembership)[nextMember])
        {
            (*viewMembership)[nextMember] = true;
            memberCount++;
        }
    }

    qDebug() << "Server membership: " << viewNum << *viewMembership;
    return viewMembership;
}


/*
void SFTViewManager::startViewChangeProposal(int viewNum)
{
    //TODO: Maybe start some sort of timer
    this->viewNum = viewNum;
}*/

/*
QList<int> SFTViewManager::proposeViewChanges(int n)
{
    QList<int> *proposedChanges = new QList<int>();
    int minViewNum = this->viewNum + 1; //View change must be at least 1 greater than current view
    for (int i = 0; i < n; i++)
    {
        int proposal = proposeViewChange(minViewNum);
        proposedChanges->append(proposal);
        minViewNum = proposal + 1;
    }
    return *proposedChanges;
}*/

//Maybe scrap this if we don't actually have access to the PublicIdentity's
/*
const Identity::Roster SFTViewManager::calcView(int viewNum)
{
    std::vector<bool> viewMembership = calcServerMembership(viewNum);
    QVector<Identity::PublicIdentity> view = new QVector<Identity::PublicIdentity>();

    for (int i = 0; i < this->numServers; i++)
    {
        if (viewMembership[i])
        {
            view.append();
        }
    }

    const Identity::Roster temp = Identity.Roster(this->m_servers);
}
*/

} //Closes SFT Namespace
} //Closes Dissent Namespace
