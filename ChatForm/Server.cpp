#ifndef UNICODE
#define UNICODE
#endif

#include <boost/asio.hpp>

#include "dllmain.hpp"

#include <iostream>
#include <string>
#include <thread>
#include <memory>

class Service
{
public:
	Service(std::shared_ptr<boost::asio::ip::tcp::socket>& sock, std::shared_ptr<Chat>& c) :
		socket(sock), 
		chat(c)
	{
		
	}

	void StartHandling()
	{
		isRunning.store(true);
		writeThread.reset(new std::thread(&Service::WriteFunction, this));

		ReadFunction();
	}

	void Stop()
	{
		OnFinish();
	}

private:

	void ReadFunction()
	{
		boost::asio::async_read_until(*socket.get(), clientBuff, '\n',
			[this](const boost::system::error_code& ec, std::size_t bytes)
			{
				if (ec.failed())
				{
					std::cout << "Can't read from socket " << ec.message() << std::endl;
					Stop();
					return;
				}

				std::istream input(&clientBuff);
				std::getline(input, clientStr);

				chat->AddStrToDialog(chat->GetHwnd(), std::wstring(clientStr.begin(), clientStr.end()), L"Client");

				std::cout << "Client: " << clientStr << std::endl;

				if (isRunning)
					ReadFunction();
			});
	}

	void WriteFunction()
	{
		if (!isRunning)
			return;

		// Add wait variable
		// Variable will be triggered by winapi flag
		{
			
			std::unique_lock<std::mutex> lock(condMutex);
			cv.wait(lock);
		}

		std::wstring text = chat->GetEnteredText();
		std::string singleStr(text.begin(), text.end());
		std::cout << "Server: " << singleStr << std::endl;

		singleStr += '\n';

		if (!isRunning)
			return;

		boost::asio::async_write(*socket.get(), boost::asio::buffer(singleStr),
			[this](const boost::system::error_code& ec, std::size_t bytes)
			{
				if (ec.failed())
				{
					std::cout << "Error writing to socket" << std::endl;
					Stop();
					return;
				}

				std::cout << "Message has been written" << std::endl;

			});
		WriteFunction();
	}

	void OnFinish()
	{
		isRunning.store(false);
		
		if (writeThread->joinable())
			writeThread->join();

		std::cout << "Cleared" << std::endl;
	}

	std::shared_ptr<boost::asio::ip::tcp::socket> socket;

	std::shared_ptr<Chat>& chat;
	std::unique_ptr<std::thread> writeThread;
	std::atomic<bool> isRunning;

	boost::asio::streambuf clientBuff;
	std::string clientStr;

};


class Server
{
public:
	Server(boost::asio::io_context& context, std::shared_ptr<Chat>& c, std::string ipAddr, int port) :
		ioContext(context),
		acceptor(ioContext, boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(ipAddr), port)),
		chat(c)
	{
		std::cout << "Constructor" << std::endl;
		work.reset(new boost::asio::io_context::work(ioContext));
	}

	void Start()
	{
		acceptor.listen();

		clientSocket = std::make_shared<boost::asio::ip::tcp::socket>(ioContext);

		std::cout << "Waiting for connection" << std::endl;

		acceptor.async_accept(*clientSocket.get(), [this](const boost::system::error_code& ec)
			{
				if (ec.failed())
				{
					std::cout << "Error accepting client" << std::endl;
					Stop();
					return;
				}

		std::cout << "Client has been connected" << std::endl;
		chat->SetIsConnected(true);
		chat->AddSystemText("Connected");

		if (isRunning)
			OnConnected();
		});
	}

	void Stop()
	{
		if (!isRunning)
			return;

		OnFinish();
	}

private:

	void OnConnected()
	{
		service = std::make_unique<Service>(clientSocket, chat);

		service->StartHandling();
	}

	void OnFinish()
	{

		std::cout << "Inside OnFinish" << std::endl;
		isRunning.store(false);
		ioContext.stop();


		work.reset(NULL);
		acceptor.close();

	}

	boost::asio::io_context& ioContext;
	boost::asio::ip::tcp::acceptor acceptor;
	std::unique_ptr<boost::asio::io_context::work> work;
	std::shared_ptr<boost::asio::ip::tcp::socket> clientSocket;

	std::shared_ptr<Chat>& chat;
	std::unique_ptr<Service> service;
	std::atomic<bool> isRunning = true;
};

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR nCmdLine, int nCmdShow)
{

	std::shared_ptr<Chat> chat(std::make_shared<Chat>(hInstance, nCmdShow, L"ServerChat"));
	
	boost::asio::io_context ioContext{ 2 };

	Server server(ioContext, chat, "127.0.0.1", 3333);

	try
	{
		// Start server
		server.Start();
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