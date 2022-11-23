#include <boost/asio.hpp>

#include <iostream>
#include <string>
#include <thread>
#include <memory>

class Client
{
public:
	Client(boost::asio::io_context& context, std::string ip, int portNumber) :
		ioContext(context),
		ipAddr(ip),
		port(portNumber)
	{
		work.reset(new boost::asio::io_context::work(ioContext));
		socket.reset(new boost::asio::ip::tcp::socket(ioContext));

		/*for (int i = 0; i < numberOfThreads; i++)
		{
			std::unique_ptr<std::thread> th = std::make_unique<std::thread>([this]() {
				ioContext.run();
				});

			threads.push_back(std::move(th));
		}*/

	}

	void Start()
	{
		std::cout << "Started" << std::endl;
		socket->async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(ipAddr), port),
			[this](const boost::system::error_code& ec)
			{
				if (ec.failed())
				{
					std::cout << "Error connecting to server" << std::endl;
					Stop();
					return;
				}

		std::cout << "Connected Successfully" << std::endl;

		OnConnected();

		writeThread.reset(new std::thread(&Client::WriteFunction, this));

			});
	}

	void Stop()
	{
		std::cout << "Stop" << std::endl;
		if (!isRunning)
			return;

		OnFinish();
	}

	void OnConnected()
	{

		boost::asio::async_read_until(*socket.get(), serverBuff, '\n',
			[this](const boost::system::error_code& ec, std::size_t bytes) {
				if (ec.failed())
				{
					std::cout << "Error reading from socket " << ec.message() << std::endl;
					Stop();
					return;
				}

		std::istream input(&serverBuff);
		std::getline(input, serverStr);

		std::cout << "Server: " << serverStr << std::endl;

		if (isRunning)
			OnConnected();

		serverStr.clear();
			});

	}

	bool GetIsRunning()
	{
		return isRunning;
	}

private:

	void WriteFunction()
	{

		std::getline(std::cin, clientStr);

		if (clientStr == prevClientStr)
			return;

		std::cout << "ClientStr: " << clientStr << std::endl;

		clientStr += '\n';
		prevClientStr = clientStr;

		boost::asio::async_write(*socket.get(), boost::asio::buffer(clientStr),
			[this](const boost::system::error_code& ec, std::size_t bytes)
			{
				if (ec.failed())
				{
					std::cout << "Error writing to socket " << ec.message() << std::endl;
					Stop();
					return;
				}

		WriteFunction();
			});
	}

	void OnFinish()
	{
		std::cout << "OnFinish" << std::endl;
		isRunning.store(false);

		work.reset(NULL);
		socket->close();
		ioContext.stop();

		if (writeThread.get() != nullptr)
			writeThread->join();
	}

	boost::asio::io_context& ioContext;
	std::unique_ptr<boost::asio::io_context::work> work;
	std::unique_ptr<boost::asio::ip::tcp::socket> socket;

	std::unique_ptr<std::thread> writeThread;

	boost::asio::streambuf serverBuff;
	std::string serverStr = "";
	std::string clientStr = "";
	std::string prevClientStr = "";

	std::string ipAddr;
	int port;

	std::atomic<bool> isRunning = true;
};

int main()
{
	boost::asio::io_context ioContext{ 2 };

	try
	{

		Client client(ioContext, "127.0.0.1", 3333);
		std::cout << "Client created" << std::endl;
		client.Start();

		ioContext.run();

		client.Stop();

	}
	catch (boost::system::system_error& ec)
	{
		std::cout << "Exception caught: " << ec.code() << std::endl;
		return -1;
	}
	return 0;
}