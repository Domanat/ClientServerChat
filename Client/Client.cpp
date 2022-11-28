#include <boost/asio.hpp>

#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include "dllmain.hpp"

class Client
{
public:
	Client(boost::asio::io_context& context, std::shared_ptr<Chat>&ch, std::string ip, int portNumber) :
		ioContext(context),
		chat(ch),
		ipAddr(ip),
		port(portNumber)
	{
		work.reset(new boost::asio::io_context::work(ioContext));
		socket.reset(new boost::asio::ip::tcp::socket(ioContext));

		callbackThread.reset(new std::thread([this]() {
			ioContext.run();
		}));
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
				
				chat->SetIsConnected(true);
				chat->AddSystemText("Connected");
				
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

				chat->AddStrToDialog(chat->GetHwnd(), std::wstring(serverStr.begin(), serverStr.end()), L"Server");

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
		{
			std::unique_lock<std::mutex> lock(condMutex);
			cv.wait(lock);
		}
		
		std::wstring text = chat->GetEnteredText();
		std::string singleStr(text.begin(), text.end());
		
		clientStr.clear();
		clientStr = singleStr;

		std::cout << "Client: " << clientStr << std::endl;
		clientStr += '\n';

		boost::asio::async_write(*socket.get(), boost::asio::buffer(clientStr),
			[this](const boost::system::error_code& ec, std::size_t bytes)
			{
				if (ec.failed())
				{
					std::cout << "Error writing to socket " << ec.message() << std::endl;
					Stop();
					return;
				}
				
				std::cout << "Message written successfully: " << clientStr << std::endl;

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
	std::unique_ptr<std::thread> callbackThread;
	std::shared_ptr<Chat>& chat;
	boost::asio::streambuf serverBuff;
	std::string serverStr = "";
	std::string clientStr = "";

	std::string ipAddr;
	int port;

	std::atomic<bool> isRunning = true;
};

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR nCmdLine, int nCmdShow)
{
	std::shared_ptr<Chat> chat(std::make_shared<Chat>(hInstance, nCmdShow, L"ClientChat"));

	boost::asio::io_context ioContext{ 2 };

	Client client(ioContext, chat, "127.0.0.1", 3333);

	try
	{
		client.Start();
		// Start io processing thread
		std::thread t([&ioContext]() {
			ioContext.run();
			}); // Maybe should use join instead of detach
		// Start UI part
		chat->Start();

		t.join();
	}
	catch (boost::system::system_error& ec)
	{
		std::cout << "Error during connection: " << ec.code().message() << std::endl;

		chat->SetIsConnected(false);
	}

	return 0;
}