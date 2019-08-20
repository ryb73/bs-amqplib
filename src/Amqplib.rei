type connection;
type channel('confirm);
type confirmChannel = channel(Js.null(Js.Exn.t) => unit);

type properties = {
    replyTo: option(string),
    correlationId: option(string),
};

type rawMessage;

type message = {
    content: Node.Buffer.t,
    properties: properties,
    raw: rawMessage,
};
type exchangeInfo = { exchange: string };
type queueInfo = { queue: string, messageCount: int, consumerCount: int };

let connect: string => Js.Promise.t(connection);
let close: connection => Js.Promise.t(unit);

let createChannel: connection => Js.Promise.t(channel(unit));
let createConfirmChannel: connection => Js.Promise.t(confirmChannel);

let closeChannel: (channel(_)) => unit;
let prefetch: (int, channel(_)) => unit;

let assertExchange:
      (~durable: bool=?, string, string, channel(_)) => Js.Promise.t(exchangeInfo);

let assertQueue:
      (~durable: bool=?, ~queue: string=?, channel(_)) => Js.Promise.t(queueInfo);
let checkQueue: (string, channel(_)) => Js.Promise.t(queueInfo);
let bindQueue:
    (~queue: string, ~exchange: string, ~key: string, channel(_)) => Js.Promise.t(unit);

/** (~noAck=?, queue, callback, channel) */
let consume: (~noAck: bool=?, string, message => unit, channel(_)) => Js.Promise.t(unit);

let ack: (rawMessage, channel(_)) => unit;
let nack: (rawMessage, channel(_)) => unit;

/** (~correlationId=?, ~replyTo=?, exchange, key, content, channel) */
let publish:
    (~correlationId: string=?, ~replyTo: string=?, string, string,
      Node.Buffer.t, channel(_)) => bool;

/** (~correlationId=?, ~replyTo=?, exchange, key, content, onAck, channel) */
let publishAck:
    (~correlationId: string=?, ~replyTo: string=?, string, string,
      Node.Buffer.t, option('a) => unit, channel(Js.null('a) => unit)) => bool;

/** (~correlationId=?, ~replyTo=?, queue, content, channel) */
let sendToQueue:
    (~correlationId: string=?, ~replyTo: string=?, string,
      Node.Buffer.t, channel(_))
    => bool;

/** (~correlationId=?, ~replyTo=?, queue, content, onAck, channel) */
let sendToQueueAck:
    (~correlationId: string=?, ~replyTo: string=?, string,
      Node.Buffer.t, option(Js.Exn.t) => unit, confirmChannel)
    => bool;
