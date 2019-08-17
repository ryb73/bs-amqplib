type connection;
type channel('confirm);

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

let createChannel: connection => Js.Promise.t(channel(unit));
let createConfirmChannel:
    connection => Js.Promise.t(channel(Js.null(Js.Exn.t) => unit));

let closeChannel: (channel(_)) => unit;
let prefetch: (int, channel(_)) => unit;

let assertExchange: (string, string, channel(_)) => Js.Promise.t(exchangeInfo);

let assertQueue: (~queue: string=?, channel(_)) => Js.Promise.t(queueInfo);
let checkQueue: (string, channel(_)) => Js.Promise.t(queueInfo);
let bindQueue:
    (~queue: string, ~exchange: string, ~key: string, channel(_)) => Js.Promise.t(unit);

let consume: (~noAck: bool=?, string, message => unit, channel(_)) => Js.Promise.t(unit);

let publish:
    (~correlationId: string=?, ~replyTo: string=?, string, string,
      Node.Buffer.t, channel(_)) => bool;
let publishAck:
    (~correlationId: string=?, ~replyTo: string=?, string, string,
      Node.Buffer.t, option('a) => unit, channel(Js.null('a) => unit)) => bool;

let sendToQueue:
    (~correlationId: string=?, ~replyTo: string=?, string,
      Node.Buffer.t, channel(_))
    => bool;
let sendToQueueAck:
    (~correlationId: string=?, ~replyTo: string=?, string,
      Node.Buffer.t, option('a) => unit, channel(Js.null('a) => unit))
    => bool;
