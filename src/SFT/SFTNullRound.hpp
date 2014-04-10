#ifndef SFTNULLROUND_HPP
#define SFTNULLROUND_HPP

#include "Anonymity/Round.hpp"
#include "SFTMessageManager.hpp"
#include "SFTViewManager.hpp"

namespace Dissent {
namespace Anonymity {
    class Round;
}

namespace SFT {
  class SFTViewManager;
  class SFTMessageManager;
  /**
   * A simple Dissent exchange.  Just broadcasts everyones message to everyone else
   */
  class SFTNullRound : public Anonymity::Round {
    Q_OBJECT

    public:
      /**
       * Constructor
       * @param clients the list of clients in the round
       * @param servers the list of servers in the round
       * @param ident this participants private information
       * @param nonce Unique round id (nonce)
       * @param overlay handles message sending
       * @param get_data requests data to share during this session
       */
      explicit SFTNullRound(const Identity::Roster &clients,
          const Identity::Roster &servers,
          const Identity::PrivateIdentity &ident,
          const QByteArray &nonce,
          const QSharedPointer<ClientServer::Overlay> &overlay,
          Messaging::GetDataCallback &get_data);

      //TODO: Should name these more obviously to distinguish from MessageType
      enum RoundPhase {
          CollectionPhase,
          ClientAttendancePhase,
          ExchangeCiphersPhase,
          ViewChangeVotingPhase
      };

      /**
       * Destructor
       */
      virtual ~SFTNullRound() {}

      inline virtual QString ToString() const { return "SFTNullRound " + GetNonce().toBase64(); }

      /**
       * Handle a data message from a remote peer
       * @param from The remote sender
       * @param msg The message
       */
      virtual void ProcessPacket(const Connections::Id &from, const QByteArray &data);
      virtual void ClientProcessPacket(const Connections::Id &from, const QByteArray &data);
      virtual void ServerProcessPacket(const Connections::Id &from, const QByteArray &data);
      virtual void HandleDisconnect(const Connections::Id &id);

      quint32 messageLength;
      SFTViewManager *sftViewManager;
      SFTMessageManager *sftMessageManager;
      bool isServer; //If false, it is a client. Will seperate out into 2 class later

  public slots:
      void broadcastToServers(QVariantMap map);
      void broadcastToDownstreamClients(QVariantMap map);
      void sendToSingleNode(const Connections::Id &to, QVariantMap map);
      void pushDataOut(QByteArray data);

    protected:
      /**
       * Called when the NullRound is started
       */
      virtual void OnStart();
      virtual void OnStop();
      virtual void ClientOnStart();
      virtual void ServerOnStart();

    private:
      QVector<QByteArray> m_received;
      int m_msgs;
      QList<Connections::Id> upstreamServers;
      QList<Connections::Id> downstreamClients;
  };
}
}


#endif // SFTNULLROUND_HPP
