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



      enum RoundPhase {
          Collection,
          ClientAttendance,
          ViewChangeVoting,
          ExchangeCiphers
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

      quint32 messageLength;
      SFTViewManager *sftViewManager;
      SFTMessageManager *sftMessageManager;
      bool isServer; //If false, it is a client. Will seperate out into 2 class later

      void broadcastToServers(QVariantMap map);
      void broadcastToClients(QVariantMap map);

    protected:
      /**
       * Called when the NullRound is started
       */
      virtual void OnStart();
      virtual void ClientOnStart();
      virtual void ServerOnStart();

    private:
      /**
       * Don't receive from a remote peer more than once...
       */
      QVector<QByteArray> m_received;
      int m_msgs;

      int messageSize = 3; //Could've picked anything for here

  };
}
}


#endif // SFTNULLROUND_HPP
