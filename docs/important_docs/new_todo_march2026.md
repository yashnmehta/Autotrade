TODO List

- market watch greeks calculation should be integrated with pricestore insted of seprate calculation


- ✅ market watch sorting mechenism should be optimised (DONE 4 March 2026 - sorting now one-shot only, no auto-re-sort)


- add delta portfolio optimise as it freezing the ui 


- optimise trading view with pratham implementation

- optimise market movement with harshil 

- optimise focus manegement with identifying root cuase for current focus switch order 

- for all window context menu standardisation and optimisation

- some times snapquote window shows garbage values in depth book (best 5 data)


- open chart from market watch should open chart with that scrip and not default chart


- tradingview window taking time to load and open 



- analyse in depth how shortcut , event filter and key press events are being handled in the codebase and identify optimisations and standardisation opportunities. how scope of the shortcut is defined . what if parent widget and child widget both have same shortcut defined. how to make it work in a standard way across the codebase.


- adv market watch (custom contract spread or pair)- badla watch, pair watch (with multipy factor for example 1 banknifty to 2 Nifty where banknifty bid 58900 and ask 59000 then nifty bid should be 24450 and ask 24500 so badla watch bid 58900 - (24500*2) = 58900 - 49000 = 9900 and ask 59000 - (24450*2) = 59000 - 48900 = 10100) . same way for pair total basis nifty atm ce and pe where bid for custom contract should be bid for ce +  bid for pe and ask for custom contract should be ask for ce + ask for pe. this will be useful for traders who want to trade based on total premium of ce+pe or want to trade badla watch based on difference between future and underlying. this will be part of market watch and user can add custom contracts with formula based on existing scrips in adv market watch. adv market watch will be separate from regular market watch and user can add custom contracts with formula based on existing scrips in adv market watch. 


