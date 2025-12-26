


- Use Qt ONLY for UI, use native C++ for:
- insted of using signal slot , we will use callbacks
- optimising model.datachanged .emit
- remove dummy and test data from masters while master proceessing 
- save master at download master 
- load master at splash screen 
- indow context initialization and preferences.







i want to clarify that udp broadcast reader and xts market data socket are both different things, they have different use cases,

- they can be run on fallbacks of other or
- they can be run  simulteneously as per the use case.

one more thing is that xts market data socket will be one object serving market data of all the exchange and segment , where in udp reader, different objects are to be created for all the exchange segments.

wether it is xts market data socket,interactive socket or any udp readers all should be run on their seprate threads
###################################





market wa