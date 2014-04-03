#ifndef SFTVIEWMANAGER_H
#define SFTVIEWMANAGER_H

#include "Anonymity/Round.hpp" //Should really change this to the includes that I need instead of just copying Round's

namespace Dissent{

namespace Connections {
  class Connection;
  class Id;
}

namespace Identity {
    class Roster;
    class PrivateIdentity;
    class PublicIdentity;
}

namespace SFT {
class SFTViewManager : public QObject
{
    Q_OBJECT

public:
    SFTViewManager(const Identity::Roster &servers,
                   const Identity::PrivateIdentity &ident,
                   double quorumRatio = 2.0/3.0);

    int getCurrentViewNum();
    //const Identity::Roster getCurrentView();
    bool inCurrentView(const Connections::Id &nodeId);

    void startViewChangeProposal(int viewNum);
    int addViewChangeVote(bool vote);
    bool setNewView(int viewNum);

    QList<int> proposeViewChanges(int n);

    bool tooFewServers(); //If this is true then we should kill the round
    bool addFailedServer(const Connections::Id &nodeId); //returns true if we need a view change

private:
    int viewNum;
    QVector<bool> *currentView;
    int numServers;
    const Identity::Roster m_servers;
    const Identity::PrivateIdentity m_ident;

    double quorumRatio;
    int proposedNewView;
    int numApproves;

    QVector<Connections::Id> *downServers;

    int proposeViewChange(int minViewNum); //Starting from @minViewNum give a view that does not contain any bad servers
    QVector<bool> *calcServerMembership(int viewNum);

};

} //Closes SFT Namespace
} //Closes Dissent Namespace
#endif // SFTVIEWMANAGER_H
