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
type consumeInfo = { consumerTag: string };

let connect: (~options: Js.t(_)=?, string) => Js.Promise.t(connection);
let close: connection => Js.Promise.t(unit);

let createChannel: connection => Js.Promise.t(channel(unit));
let createConfirmChannel: connection => Js.Promise.t(confirmChannel);

let closeChannel: (channel(_)) => unit;
let prefetch: (channel(_), int) => unit;

let assertExchange:
      (~durable: bool=?, channel(_), string, string) => Js.Promise.t(exchangeInfo);

let assertQueue:
      (~durable: bool=?, ~queue: string=?, channel(_)) => Js.Promise.t(queueInfo);
let checkQueue: (channel(_), string) => Js.Promise.t(queueInfo);
let bindQueue:
    (channel(_), ~queue: string, ~exchange: string, ~key: string) => Js.Promise.t(unit);
let purgeQueue: (channel(_), string) => Js.Promise.t(unit);

/** (~noAck=?, queue, callback, channel) */
let consume:
      (~noAck: bool=?, channel(_), string, message => unit) => Js.Promise.t(consumeInfo);

/** (consumerTag, channel) */
let cancel: (channel(_), string) => Js.Promise.t(unit);

let ack: (channel(_), rawMessage) => unit;
let nack: (channel(_), rawMessage) => unit;

/** (~correlationId=?, ~replyTo=?, exchange, key, content, channel) */
let publish:
    (~correlationId: string=?, ~replyTo: string=?, channel(_), string, string,
      Node.Buffer.t) => bool;

/** (~correlationId=?, ~replyTo=?, exchange, key, content, onAck, channel) */
let publishAck:
    (~correlationId: string=?, ~replyTo: string=?, confirmChannel,
      string, string, Node.Buffer.t, option(Js.Exn.t) => unit)
    => bool;

/** (~correlationId=?, ~replyTo=?, queue, content, channel) */
let sendToQueue:
    (~correlationId: string=?, ~replyTo: string=?, channel(_), string, Node.Buffer.t)
    => bool;

/** (~correlationId=?, ~replyTo=?, queue, content, onAck, channel) */
let sendToQueueAck:
    (~correlationId: string=?, ~replyTo: string=?, confirmChannel, string,
      Node.Buffer.t, option(Js.Exn.t) => unit)
    => bool;
