#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <vector>

using boost::asio::ip::tcp;

void play_game(tcp::socket &socket) {
  std::string reply;

  try {
    while (true) {
      std::vector<char> buffer(1024);
      size_t len = socket.read_some(boost::asio::buffer(buffer));

      reply = std::string(buffer.begin(), buffer.begin() + len);
      std::cout << reply << std::endl;

      if (reply.find("won") != std::string::npos ||
          reply.find("lose") != std::string::npos ||
          reply.find("draw") != std::string::npos) {
        break;
      }

      if (reply.find("Enter your turn:") != std::string::npos ||
          reply.find("Invalid") != std::string::npos) {
        std::string move;
        std::cout << "Your move: ";
        std::cin >> move;

        boost::asio::write(socket, boost::asio::buffer(move + "\n"));
      }
    }
  } catch (boost::system::system_error &e) {
    if (e.code() != boost::asio::error::eof) {
      std::cerr << "Error during game play: " << e.what() << std::endl;
    } else {
      std::cout << "Connection closed by server. Game ended." << std::endl;
    }
  } catch (std::exception &e) {
    std::cerr << "Unexpected error: " << e.what() << std::endl;
  }
}

int main(int argc, char *argv[]) {
  try {
    if (argc != 2) {
      std::cerr << "Usage: client <host>\n";
      return 1;
    }

    boost::asio::io_context io_context;
    tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve(argv[1], "12345");
    tcp::socket socket(io_context);
    boost::asio::connect(socket, endpoints);

    std::cout << "Connected to server. Starting the game...\n";
    play_game(socket);
  } catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }

  return 0;
}
