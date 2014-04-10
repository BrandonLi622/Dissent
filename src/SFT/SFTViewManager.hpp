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
                   double quorumRatio = 2.0/3.0);

    //Accessor / State information
    int getCurrentViewNum();
    int getProposedViewNum();
    bool inCurrentView(const Connections::Id &nodeId);
    int getViewSize();
    bool tooFewServers(); //If this is true then we should kill the round
    QVector<Connections::Id> getCurrentServers();

    //Modifiers
    bool addFailedServer(const Connections::Id &nodeId); //returns true if we need a view change
    int nextGoodView(int minViewNum); //Starting from @minViewNum give a view that does not contain any bad servers
    int addViewChangeVote(int viewNum, bool vote, const Connections::Id &voter);
    bool setNewView(int viewNum);
    void proposeNewView(int viewNum);


    //void startViewChangeProposal(int viewNum);
    //QList<int> proposeViewChanges(int n);

private:
    int viewNum;
    int proposedViewNum;
    QVector<bool> *currentView;
    int numServers;
    const Identity::Roster m_servers;

    QHash<int, QVariantMap> *viewChangeProposals;
    QVector<Connections::Id> *downServers;
    QVector<bool> *calcServerMembership(int viewNum);

    double quorumRatio;

};

} //Closes SFT Namespace
} //Closes Dissent Namespace
#endif // SFTVIEWMANAGER_H
