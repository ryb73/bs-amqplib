type connection;
type channel;

[@decco] type message = { content: Node.Buffer.t };
[@decco] type exchangeInfo = { exchange: string };
[@decco] type queueInfo = { queue: string, messageCount: int, consumerCount: int };

let connect: string => Js.Promise.t(connection);
let close: connection => Js.Promise.t(unit);
let createChannel: connection => Js.Promise.t(channel);
let assertExchange: (string, string, channel) => Js.Promise.t(exchangeInfo);
let assertQueue: (~queue: string=?, channel) => Js.Promise.t(queueInfo);
let checkQueue: (string, channel) => Js.Promise.t(queueInfo);
let bindQueue:
    (~queue: string, ~exchange: string, ~key: string, channel) => Js.Promise.t(unit);
let consume: (string, message => unit, channel) => Js.Promise.t(unit);
let publish: (string, string, Node.Buffer.t, channel) => Js.Promise.t(bool);
let sendToQueue: (string, Node.Buffer.t, channel) => Js.Promise.t(bool);
let prefetch: (int, channel) => unit;
