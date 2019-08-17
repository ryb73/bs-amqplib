type connection;
type channel;

type properties = {
    replyTo: option(string),
    correlationId: option(string),
};

type message = {
    content: Node.Buffer.t,
    properties: properties,
};
type exchangeInfo = { exchange: string };
type queueInfo = { queue: string, messageCount: int, consumerCount: int };

let connect: string => Js.Promise.t(connection);
let close: connection => Js.Promise.t(unit);
let createChannel: connection => Js.Promise.t(channel);
let assertExchange: (string, string, channel) => Js.Promise.t(exchangeInfo);
let assertQueue: (~queue: string=?, channel) => Js.Promise.t(queueInfo);
let checkQueue: (string, channel) => Js.Promise.t(queueInfo);
let bindQueue:
    (~queue: string, ~exchange: string, ~key: string, channel) => Js.Promise.t(unit);
let consume: (~noAck: bool=?, string, message => unit, channel) => Js.Promise.t(unit);
let publish:
    (~correlationId: string=?, ~replyTo: string=?, string, string,
      Node.Buffer.t, channel) => bool;
let sendToQueue:
    (~correlationId: string=?, ~replyTo: string=?, string, Node.Buffer.t, channel)
    => bool;
let prefetch: (int, channel) => unit;
