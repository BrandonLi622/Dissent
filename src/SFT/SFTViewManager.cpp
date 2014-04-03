#include "SFTViewManager.hpp"

namespace Dissent{
namespace SFT {
SFTViewManager::SFTViewManager(const Identity::Roster &servers,
                               const Identity::PrivateIdentity &ident,
                               double quorumRatio) :
    m_servers(servers),
    m_ident(ident)
{
    this->downServers = new QVector<Connections::Id>();
    this->numServers = m_servers.Count();
    this->quorumRatio = quorumRatio;
    this->viewNum = 0;
    this->currentView = new QVector<bool>();
    setNewView(this->viewNum);

    qDebug() << "SFTViewManager constructor IS WORKING" << *(this->currentView);
    //qDebug() << this->currentView;
}

int SFTViewManager::getCurrentViewNum()
{
    return this->viewNum;
}

bool SFTViewManager::inCurrentView(const Connections::Id &nodeId)
{
    int index = this->m_servers.GetIndex(nodeId);
    return (*(this->currentView))[index];
}

bool SFTViewManager::addFailedServer(const Connections::Id &nodeId)
{
    this->downServers->append(nodeId);
    return this->inCurrentView(nodeId);
}

int SFTViewManager::proposeViewChange(int minViewNum)
{
    QVector<bool> *viewMembership = calcServerMembership(minViewNum);
    for (int j = 0; j < this->downServers->count(); j++)
    {
        const Connections::Id downServer = downServers->at(j);
        int index = this->m_servers.GetIndex(downServer);

        //If any of the down servers are in a particular view, try the next view
        if (viewMembership->at(index))
        {
            proposeViewChange(minViewNum + 1);
        }
    }

    //If we got here none of the bad servers are in the next view
    return minViewNum;
}

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
}

bool SFTViewManager::tooFewServers()
{
    return this->downServers->count() >= 2.0 * this->numServers / 3.0;
}

void SFTViewManager::startViewChangeProposal(int viewNum)
{
    this->proposedNewView = viewNum;
    this->numApproves = 0;

    //TODO: Maybe start some sort of timer
}

int SFTViewManager::addViewChangeVote(bool vote)
{
    if (vote)
    {
        this->numApproves += 1;
    }

    if (this->numApproves >= quorumRatio * numServers)
    {
        //TODO: What if this fails?
        setNewView(this->proposedNewView);
        return this->viewNum;
    }

    return -1;
}

bool SFTViewManager::setNewView(int viewNum)
{
    delete this->currentView;
    this->viewNum = viewNum;
    this->currentView = calcServerMembership(viewNum);

    //TODO: Need to calculate the actual view

    return true;
}

QVector<bool> *SFTViewManager::calcServerMembership(int viewNum)
{
    QVector<bool> *viewMembership = new QVector<bool>();
    srand(viewNum); //TODO: is this good enough for variance?

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

    return viewMembership;
}

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
