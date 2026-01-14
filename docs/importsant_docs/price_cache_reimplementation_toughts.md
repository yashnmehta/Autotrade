 so we want to reemployment the price cash and broadcast working from scratch.



 Physically all the UDP readers will have their own cache array. Something like we have used in the contract master.

 All the feed which is deleted to market data you can share price Live price will be stored on this array.


 So we can make this error to dimensional or three dimensional as we like but I think a two array with normalised column for different type of message code will be more efficient so when the data received on UDP data will be sent to the main object using callback as well as stored in the centralised storage(caches array)


 When the subscriber object require data on for any token request will be done to the feed handler feed handler same request forward to main object main object will access the segment specific UDP object and its cash to supply the information to the feed handler and feed handler will provide this to the actual subscriber.


 So in the current implementation UDP continuously forwarding every data to the main object and main object will providing his data to the feed handler where feed handler to the data in his map Limited to token which are already subscribed by the end subscriber objects, here mean object is continuously providing the data to the feed handler and feed handler is providing data to the end subscriber on demand basis where in propose implementation we are suggesting that data should be stored in array or re in UDP object only call back will be there to supply the data in the application but the centralised storage to supply the on demand data will be distributed to this four different UDP readers objects so the locks can be mutex and locks can be minimise and their impact can be minimise on UI.

 What one more thing which we can add to optimise the efficiency but we should take a call on this as it will add more complexity to the implementation we can supply the subscribe token information to the UDP reader so the callback function will only continuously supply the info about the token to the main object Limited to the subscribe token only but it will add more complexity to the implementation so we have to check the actual complexity and the feasibility on this part.


 I want you to analyse every day detailed and the current implementation and provide your unbiased suggestion on this.

