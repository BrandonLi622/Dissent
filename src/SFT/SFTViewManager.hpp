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

private:
    int viewNum;
    QVector<bool> *currentView;
    bool isProposer;
    int numServers;
    const Identity::Roster m_servers;
    const Identity::PrivateIdentity m_ident;

    double quorumRatio;
    int proposedNewView;
    int numApproves;

    /*const Identity::Roster calcView(int viewNum); Maybe scrap this */
    QVector<bool> *calcServerMembership(int viewNum);

};

} //Closes SFT Namespace
} //Closes Dissent Namespace
#endif // SFTVIEWMANAGER_H
