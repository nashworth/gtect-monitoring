//   'use strict';

// This function is triggered each time a message is revieved in the IoTHub.
// The message payload is persisted in an Azure Storage Table
var moment = require('moment');

module.exports = function (context, iotHubMessage) {
   var datastring = JSON.stringify(iotHubMessage);
   var dslength = datastring.length - 5;
   datastring = datastring.substr(3, dslength);
   datastring = datastring.split("\"");
   context.log('Message received: ' + datastring);
   var deviceData = Object.assign({ "partitionKey": moment.utc().format('YYYYMMDD'),
    "rowKey": moment.utc().format('hhmmss') + process.hrtime()[1] + '' ,
     "device_id": datastring[2],
     "published_at": datastring[6],
     "event": datastring[10],
     "TC_Temp": datastring[14],
     "RoomTemp": datastring[18],
     "Acc_3200": datastring[22],
     "Acc_1600": datastring[26],
     "Acc_800": datastring[30],
     "Acc_400": datastring[34],
     "Acc_200": datastring[38],
     "Acc_100": datastring[42],
     "Acc_50": datastring[46],
     "Acc_25": datastring[50]
   })
   context.log(deviceData);
   context.bindings.deviceData = deviceData;
   context.done();
};