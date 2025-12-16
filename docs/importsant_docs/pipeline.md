untill and unless C++ native code not serves the purpose we are not intend to use Qt wrapper classes for non ui purpose . like QNetwork or QTimer as they will add up latency.

can we test how much efficincy of Qtimer or Qnetwork against native class in nenoseconds(actual test and not assumed ) ????



is there any other ui library , more latency sensitive then qt which we can use for our trading terminal ??



Use Qt ONLY for UI, use native C++ for:

WebSocket (websocketpp or Boost.Beast)
Timers (std::chrono + std::thread)
Network I/O (direct sockets)
Data processing (std::string, not QString


