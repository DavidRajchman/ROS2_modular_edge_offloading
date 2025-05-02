Use modified transport libs

After receiving offloading allowed message for certain outputs, create a maping which will forward messages from certain vehicle topics, to the MEC server. Then it will setup a listener for the output topic from MEC and forward it to the vehicle

The response will be a json which the bridge will intercept and setup the forwarding accrodingly, it will also notify the VHC and MEC server to enable the handlers

{
  "RequestID": 0,
  "GrantType": 0,
  "MECserverLOC":"",
  "InputsForProcessing": [],
  "ProcessedOutputs": [],
}