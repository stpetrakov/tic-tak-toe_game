#include <array>
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <random>
#include <string>

using boost::asio::ip::tcp;

class Board {
public:
  Board() : grid_({{'1', '2', '3', '4', '5', '6', '7', '8', '9'}}) {}

  std::string getBoardState() {
    std::string board_state = "The board currently is:\n";
    for (int i = 0; i < 9; ++i) {
      board_state += grid_[i];
      if (i % 3 == 2)
        board_state += "\n";
      else
        board_state += " ";
    }
    return board_state;
  }

  bool makeMove(int position, char player) {
    if (position < 1 || position > 9 || grid_[position - 1] == 'X' ||
        grid_[position - 1] == 'O') {
      return false;
    }
    grid_[position - 1] = player;
    return true;
  }

  int checkWin() {
    const int win_conditions[8][3] = {
        {0, 1, 2}, {3, 4, 5}, {6, 7, 8}, // строки
        {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, // столбцы
        {0, 4, 8}, {2, 4, 6}             // диагонали
    };

    for (const auto &condition : win_conditions) {
      if (grid_[condition[0]] == grid_[condition[1]] &&
          grid_[condition[1]] == grid_[condition[2]]) {
        std::cout << "Player " << ((grid_[condition[0]] == 'X') ? "1" : "2")
                  << " wins with " << std::to_string(condition[0] + 1) << "-"
                  << std::to_string(condition[1] + 1) << "-"
                  << std::to_string(condition[2] + 1) << "\n";

        return (grid_[condition[0]] == 'X') ? 1 : 2;
      }
    }

    for (char cell : grid_) {
      if (cell != 'X' && cell != 'O')
        return 0;
    }
    return -1; // Ничья
  }

  void resetBoard() { grid_ = {'1', '2', '3', '4', '5', '6', '7', '8', '9'}; }

  int getBotMove() {
    const int win_conditions[8][3] = {
        {0, 1, 2}, {3, 4, 5}, {6, 7, 8}, // строки
        {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, // столбцы
        {0, 4, 8}, {2, 4, 6}             // диагонали
    };

    // Проверка, может ли игрок выиграть на следующем ходу и блокировка
    for (const auto &condition : win_conditions) {
      int x_count = 0;
      int empty_pos = -1;

      // Проверяем текущую линию на 2 'X' и пустую ячейку
      for (int pos : condition) {
        if (grid_[pos] == 'X') {
          x_count++;
        } else if (grid_[pos] != 'O') {
          empty_pos = pos;
        }
      }

      // Если есть 2 'X' и одна пустая клетка, бот блокирует ход игрока
      if (x_count == 2 && empty_pos != -1) {
        return empty_pos + 1;
      }
    }

    // Если блокировка не требуется, бот делает случайный ход
    std::vector<int> available_moves;
    for (int i = 0; i < 9; ++i) {
      if (grid_[i] != 'X' && grid_[i] != 'O') {
        available_moves.push_back(i + 1);
      }
    }
    if (available_moves.empty())
      return -1; // Нет доступных ходов

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, available_moves.size() - 1);

    return available_moves[dis(gen)];
  }

private:
  std::array<char, 9> grid_;
};

class GameSession : public std::enable_shared_from_this<GameSession> {
public:
  GameSession(boost::asio::io_context &io_context, int session_number)
      : socket1_(io_context), socket2_(io_context), strand_(io_context),
        session_number_(session_number) {}

  tcp::socket &player1_socket() { return socket1_; }
  tcp::socket &player2_socket() { return socket2_; }
  void setPlayingWithBot(bool value) { playing_with_bot_ = value; }
  bool returnPlayingWithBot() { return playing_with_bot_; }

  void start() {
    board_.resetBoard();
    std::string message1 = "You are playing crosses (X). Good luck!\n";
    deliver(message1, socket1_);

    std::string board_state = board_.getBoardState();
    deliver(board_state, socket1_);

    if (playing_with_bot_) {
      deliver("You are playing against the bot.\n", socket1_);
      deliver("Enter your turn: ", socket1_);
      readMessage(socket1_, 1);
    } else {
      deliver("You are playing noughts (O). Good luck!\n", socket2_);
      deliver(board_state, socket2_);
      deliver("Enter your turn: ", socket1_);
      deliver("Please wait for the other player's turn...\n", socket2_);
      readMessage(socket1_, 1);
    }
  }

  void processMove(const std::string &moveStr, int player) {
    try {
      int position = std::stoi(moveStr);
      char mark = (player == 1) ? 'X' : 'O';

      if (board_.makeMove(position, mark)) {
        int winner = board_.checkWin();
        std::string boardState = board_.getBoardState();

        if (winner > 0) {
          deliver(boardState, socket1_);
          if (!playing_with_bot_) {
            deliver(boardState, socket2_);
          }
          deliver("Congratulations! You won!\n",
                  player == 1 ? socket1_ : socket2_);
          if (!playing_with_bot_) {
            deliver("Sorry! You lose!\n", player == 1 ? socket2_ : socket1_);
          }
        } else if (winner == -1) {
          boardState += "It's a draw!\n";
          deliver(boardState, socket1_);
          if (!playing_with_bot_) {
            deliver(boardState, socket2_);
          }
        } else {
          deliver(boardState, socket1_);
          if (!playing_with_bot_) {
            deliver(boardState, socket2_);
          }

          if (playing_with_bot_ && player == 1) {
            makeBotMove();
          } else {
            deliver("Please wait for the other player's turn...\n",
                    player == 1 ? socket1_ : socket2_);
            deliver("Enter your turn: ", player == 1 ? socket2_ : socket1_);
            readMessage(player == 1 ? socket2_ : socket1_, player == 1 ? 2 : 1);
          }
        }
      } else {
        deliver("Invalid move. Try again.\nEnter your turn: ",
                player == 1 ? socket1_ : socket2_);
        readMessage(player == 1 ? socket1_ : socket2_, player);
      }
    } catch (...) {
      deliver("Invalid input. Please enter a number.\nEnter your turn: ",
              player == 1 ? socket1_ : socket2_);
      readMessage(player == 1 ? socket1_ : socket2_, player);
    }
  }

  void makeBotMove() {
    int botMove = board_.getBotMove();
    if (botMove == -1)
      return; // Нет доступных ходов

    board_.makeMove(botMove, 'O');
    std::string boardState = board_.getBoardState();
    deliver(boardState, socket1_);
    deliver("Bot's move: " + std::to_string(botMove) + "\n", socket1_);

    int winner = board_.checkWin();
    if (winner > 0) {
      deliver("Sorry! The bot won!\n", socket1_);
    } else if (winner == -1) {
      deliver("It's a draw!\n", socket1_);
    } else {
      deliver("Enter your turn: ", socket1_);
      readMessage(socket1_, 1);
    }
  }

  void deliver(const std::string &msg, tcp::socket &socket) {
    auto self(shared_from_this());
    boost::asio::post(strand_, [this, self, msg, &socket]() {
      boost::asio::async_write(
          socket, boost::asio::buffer(msg),
          [this, self, &socket](boost::system::error_code ec, std::size_t) {
            if (ec) {
              socket.close();
            }
          });
    });
  }

  int getSessionNumber() { return session_number_; }

private:
  void readMessage(tcp::socket &socket, int player) {
    auto self(shared_from_this());
    boost::asio::async_read_until(
        socket, boost::asio::dynamic_buffer(data_), '\n',
        boost::asio::bind_executor(
            strand_, [this, self, &socket, player](boost::system::error_code ec,
                                                   std::size_t length) {
              if (!ec) {
                std::string message = data_.substr(0, length - 1);
                data_.erase(0, length);

                std::cout << "Session " << session_number_
                          << " - Received from player " << player << ": "
                          << message << std::endl;
                processMove(message, player);
              } else {
                std::cerr << "Error on receive: " << ec.message() << std::endl;
                socket.close();
              }
            }));
  }

  tcp::socket socket1_;
  tcp::socket socket2_;
  boost::asio::io_context::strand strand_;
  std::string data_;
  Board board_;
  int session_number_;
  bool playing_with_bot_ =
      false; // Флаг, указывающий, играет ли игрок против бота
};

class GameServer {
public:
  GameServer(boost::asio::io_context &io_context, int port)
      : io_context(io_context),
        acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
    server_port = port;
    startAccept();
  }

private:
  void startAccept() {
    std::cout << "TicTacToe server version 1.0.0\nListening for incoming "
                 "connections on port "
              << std::to_string(server_port) << "...\n";

    auto session =
        std::make_shared<GameSession>(std::ref(io_context), number_session);

    acceptor_.async_accept(
        session->player1_socket(),
        [this, session](boost::system::error_code ec) {
          if (!ec) {
            std::cout << "Player 1 connected in session "
                      << session->getSessionNumber() << "...\n";
            session->deliver("Welcome to the TicTacToe server version "
                             "1.0.0!\nYou are the first player\n"
                             "Please wait for the other player to connect...\n",
                             session->player1_socket());

            auto timer = std::make_shared<boost::asio::steady_timer>(
                io_context, std::chrono::seconds(5));
            timer->async_wait([this, session,
                               timer](const boost::system::error_code &error) {
              if (!error && !session->player2_socket().is_open()) {
                std::cout << "Starting game with bot for player 1 in session "
                          << session->getSessionNumber() << ".\n";
                session->setPlayingWithBot(true);
                session->start();
              }
            });

            acceptor_.async_accept(
                session->player2_socket(),
                [this, session, timer](boost::system::error_code ec) {
                  timer->cancel();
                  if (!session->returnPlayingWithBot() && !ec) {
                    std::cout << "Player 2 connected in session "
                              << session->getSessionNumber()
                              << ". Starting game.\n";
                    session->deliver("Welcome to the TicTacToe server version "
                                     "1.0.0!\nYou are the second player\n",
                                     session->player2_socket());
                    session->start();
                  } else if (session->returnPlayingWithBot()) {
                    std::cout << "Game already started with bot for session "
                              << session->getSessionNumber()
                              << ". Closing connection for Player 2.\n";
                    session->player2_socket().close();
                  } else {
                    std::cerr << "Error accepting player 2: " << ec.message()
                              << "\n";
                  }
                });
            startAccept();
          } else {
            std::cerr << "Error accepting player 1: " << ec.message() << "\n";
            startAccept();
          }
        });
  }

  boost::asio::io_context &io_context;
  tcp::acceptor acceptor_;
  int number_session = 1;
  int server_port = 0;
};

int main(int argc, char *argv[]) {
  try {
    if (argc != 2) {
      std::cerr << "Usage: server <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;
    GameServer server(io_context, std::atoi(argv[1]));
    io_context.run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}