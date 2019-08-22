open PromEx;

type connection;
type channel('confirm);
type confirmChannel = channel(Js.null(Js.Exn.t) => unit);

let decodeUndefined = (decoder, json) =>
    (json === [%bs.raw "undefined"])
        ? Belt.Result.Ok(None)
        : Decco.optionFromJson(decoder, json);

[@decco.decode]
type properties = {
    replyTo: [@decco.codec (_=>failwith("no encode"), decodeUndefined)] option(string),
    correlationId: [@decco.codec (_=>failwith("no encode"), decodeUndefined)] option(string),
};

[@decco.decode] type rawMessage = Js.Json.t;

[@decco.decode]
type message = {
    content: [@decco.codec Decco.Codecs.magic] Node.Buffer.t,
    properties: properties,
    raw: rawMessage,
};

[@decco.decode]
type exchangeInfo = {
    exchange: string,
};

[@decco.decode]
type queueInfo = {
    queue: string,
    messageCount: int,
    consumerCount: int,
};

[@decco.decode] type consumeInfo = { consumerTag: string };

[@bs.module "amqplib"] external connect: string => Js.Promise.t(connection) = "";

[@bs.send.pipe: connection] external close: Js.Promise.t(unit) = "";

[@bs.send.pipe: connection] external createChannel: Js.Promise.t(channel(unit)) = "";
[@bs.send.pipe: connection]
external createConfirmChannel: Js.Promise.t(confirmChannel) = "";

[@bs.send.pipe: channel(_)] external closeChannel: unit = "close";
[@bs.send] external prefetch: (channel(_), int) => unit = "";

type exchangeOptions;
[@bs.obj] external exchangeOptions: (~durable: bool=?, unit) => exchangeOptions = "";

[@bs.send]
external assertExchange:
    (channel(_), string, string, exchangeOptions) => Js.Promise.t(Js.Json.t) = "";
let assertExchange = (~durable=?, channel, exchange, type_) =>
    exchangeOptions(~durable?, ())
    |> assertExchange(channel, exchange, type_)
    |> map(exchangeInfo_decode)
    |> map(Belt.Result.getExn);

type queueOptions;
[@bs.obj] external queueOptions: (~durable: bool=?, unit) => queueOptions = "";

[@bs.send]
external assertQueue:
    (channel(_), ~queue: string=?, queueOptions) => Js.Promise.t(Js.Json.t) = "";
let assertQueue = (~durable=?, ~queue=?, channel) =>
    queueOptions(~durable?, ())
    |> assertQueue(~queue?, channel)
    |> map(queueInfo_decode)
    |> map(Belt.Result.getExn);

[@bs.send]
external checkQueue: (channel(_), string) => Js.Promise.t(Js.Json.t) = "";
let checkQueue = (channel, queue) =>
    checkQueue(channel, queue)
    |> map(queueInfo_decode)
    |> map(Belt.Result.getExn);

[@bs.send]
external bindQueue:
    (channel(_), ~queue: string, ~exchange: string, ~key: string)
    => Js.Promise.t(unit) = "";

[@bs.send] external purgeQueue: (channel(_), string) => Js.Promise.t(unit) = "";

type consumeOptions;
[@bs.obj] external consumeOptions: (~noAck: bool=?, unit) => consumeOptions = "";

[@bs.send]
external consume:
    (channel(_), string, Js.Json.t => unit, consumeOptions)
    => Js.Promise.t(Js.Json.t) = "";
let consume = (~noAck=?, channel, queue, cb) =>
    consumeOptions(~noAck?, ())
    |> consume(channel, queue, json =>
        message_decode(json)
        |> Belt.Result.getExn
        |> (msg => { ...msg, raw: json })
        |> cb,
    )
    |> map(consumeInfo_decode)
    |> map(Belt.Result.getExn);

[@bs.send] external cancel: (channel(_), string) => Js.Promise.t(unit) = "";

[@bs.send] external ack: (channel(_), rawMessage) => unit = "";
[@bs.send] external nack: (channel(_), rawMessage) => unit = "";

type publishOptions;
[@bs.obj]
external publishOptions:
    (~correlationId: string=?, ~replyTo: string=?, unit) => publishOptions = "";

let wrapOnAck = (onAck) =>
    Some(err => Js.nullToOption(err) |> onAck);

[@bs.send]
external publishImpl:
    (channel('confirm), string, string, Node.Buffer.t,
     publishOptions, option('confirm))
    => bool = "publish";

let publish = (~correlationId=?, ~replyTo=?, channel, exchange, key, content) =>
    publishOptions(~correlationId?, ~replyTo?, ())
    |> publishImpl(channel, exchange, key, content, _, None);

let publishAck =
    (~correlationId=?, ~replyTo=?, channel, exchange, key, content, onAck) =>
        publishOptions(~correlationId?, ~replyTo?, ())
        |> publishImpl(channel, exchange, key, content, _, wrapOnAck(onAck));

[@bs.send]
external sendToQueueImpl:
    (channel('confirm), string, Node.Buffer.t, publishOptions, option('confirm))
    => bool = "sendToQueue";

let sendToQueue = (~correlationId=?, ~replyTo=?, channel, queue, content) =>
    publishOptions(~correlationId?, ~replyTo?, ())
    |> sendToQueueImpl(channel, queue, content, _, None);

let sendToQueueAck = (~correlationId=?, ~replyTo=?, channel, queue, content, onAck) =>
    publishOptions(~correlationId?, ~replyTo?, ())
    |> sendToQueueImpl(channel, queue, content, _, wrapOnAck(onAck));
