#ifndef __COMMANDHANDLER_H__
#define __COMMANDHANDLER_H__

#include <set>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

class CommandHandler;

class CommandConnection : public boost::enable_shared_from_this<CommandConnection>,
			  private boost::noncopyable
{
    public:
	typedef boost::shared_ptr<CommandConnection> Ptr;

    public:
	CommandConnection(CommandHandler& handler,
			  boost::asio::io_service& ioService);

    public:
	boost::asio::ip::tcp::socket& socket() {
	    return m_socket;
	}
	void startRead() {
	    m_socket.async_read_some(boost::asio::buffer(m_buffer),
		boost::bind(&CommandConnection::handleRead, shared_from_this(),
			    boost::asio::placeholders::error,
			    boost::asio::placeholders::bytes_transferred));
	}
	void close() {
	    m_socket.close();
	}

    private:
	void handleRead(const boost::system::error_code& error,
			size_t bytesTransferred);
	void handleWrite(const boost::system::error_code& error);
	void respond(const std::string& response) {
	    boost::asio::async_write(m_socket, boost::asio::buffer(response + "\n"),
		boost::bind(&CommandConnection::handleWrite, shared_from_this(),
			    boost::asio::placeholders::error));
	}
	bool handleCommand(const std::string& command);

    private:
	boost::asio::ip::tcp::socket m_socket;
	boost::array<char, 1024> m_buffer;
	std::string m_pendingCommand;
	CommandHandler& m_handler;
};

class CommandHandler : private boost::noncopyable
{
    public:
	CommandHandler(boost::asio::io_service& ioService,
		       boost::asio::ip::tcp::endpoint& endpoint);
	~CommandHandler();

    public:
	void startConnection(CommandConnection::Ptr connection);
	void stopConnection(CommandConnection::Ptr connection);

    private:
	void handleAccept(CommandConnection::Ptr connection,
			  const boost::system::error_code& error);
	void startAccepting();

    private:
	boost::asio::io_service& m_service;
	boost::asio::ip::tcp::acceptor m_acceptor;
	std::set<CommandConnection::Ptr> m_connections;
};

#endif /* __COMMANDHANDLER_H__ */
