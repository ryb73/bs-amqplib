open PromEx;

type connection;
type channel('confirm);

let decodeUndefined = (decoder, json) =>
    (json === [%bs.raw "undefined"])
        ? Belt.Result.Ok(None)
        : Decco.optionFromJson(decoder, json);

[@decco.decode]
type properties = {
    replyTo: [@decco.codec (_=>failwith("no encode"), decodeUndefined)] option(string),
    correlationId: [@decco.codec (_=>failwith("no encode"), decodeUndefined)] option(string),
};

[@decco.decode]
type message = {
    content: [@decco.codec Decco.Codecs.magic] Node.Buffer.t,
    properties: properties,
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

[@bs.module "amqplib"] external connect: string => Js.Promise.t(connection) = "";

[@bs.send.pipe: connection] external close: Js.Promise.t(unit) = "";

[@bs.send.pipe: connection] external createChannel: Js.Promise.t(channel(unit)) = "";
[@bs.send.pipe: connection]
external createConfirmChannel:
    Js.Promise.t(channel(Js.null(Js.Exn.t) => unit)) = "";

[@bs.send.pipe: channel(_)] external closeChannel: unit = "close";
[@bs.send.pipe: channel(_)] external prefetch: int => unit = "";

[@bs.send.pipe: channel('a)]
external assertExchange: (string, string) => Js.Promise.t(Js.Json.t) = "";
let assertExchange = (exchange, type_, channel) =>
    assertExchange(exchange, type_, channel)
    |> map(exchangeInfo_decode)
    |> map(Belt.Result.getExn);

[@bs.send.pipe: channel('a)]
external assertQueue: (~queue: string=?) => Js.Promise.t(Js.Json.t) = "";
let assertQueue = (~queue=?, channel) =>
    assertQueue(~queue?, channel)
    |> map(queueInfo_decode)
    |> map(Belt.Result.getExn);

[@bs.send.pipe: channel('a)] external checkQueue: (string) => Js.Promise.t(Js.Json.t) = "";
let checkQueue = (queue, channel) =>
    checkQueue(queue, channel)
    |> map(queueInfo_decode)
    |> map(Belt.Result.getExn);

[@bs.send.pipe: channel('a)]
external bindQueue:
    (~queue: string, ~exchange: string, ~key: string) => Js.Promise.t(unit) = "";

type consumeOptions;
[@bs.obj] external consumeOptions: (~noAck: bool=?, unit) => consumeOptions = "";

[@bs.send.pipe: channel('s)]
external consume: (string, Js.Json.t => unit, consumeOptions) => Js.Promise.t(unit) = "";
let consume = (~noAck=?, queue, cb) =>
    consumeOptions(~noAck?, ())
    |> consume(queue, json =>
        message_decode(json)
        |> Belt.Result.getExn
        |> cb
    );

type publishOptions;
[@bs.obj]
external publishOptions:
    (~correlationId: string=?, ~replyTo: string=?, unit) => publishOptions = "";

let wrapOnAck = (onAck) =>
    (err) => Js.nullToOption(err) |> onAck;

[@bs.send.pipe: channel('confirm)]
external publishImpl:
    (string, string, Node.Buffer.t, publishOptions, ~onAck: 'confirm=?)
    => bool = "publish";

let publish = (~correlationId=?, ~replyTo=?, exchange, key, content, channel) =>
    publishOptions(~correlationId?, ~replyTo?, ())
    |> publishImpl(exchange, key, content, _, channel);

let publishAck =
    (~correlationId=?, ~replyTo=?, exchange, key, content, onAck, channel) =>
        publishOptions(~correlationId?, ~replyTo?, ())
        |> publishImpl(~onAck=wrapOnAck(onAck), exchange, key, content, _, channel);

[@bs.send.pipe: channel('confirm)]
external sendToQueueImpl:
    (string, Node.Buffer.t, publishOptions, ~onAck: 'confirm=?) => bool = "sendToQueue";

let sendToQueue = (~correlationId=?, ~replyTo=?, queue, content, channel) =>
    publishOptions(~correlationId?, ~replyTo?, ())
    |> sendToQueueImpl(queue, content, _, channel);

let sendToQueueAck = (~correlationId=?, ~replyTo=?, queue, content, onAck, channel) =>
    publishOptions(~correlationId?, ~replyTo?, ())
    |> sendToQueueImpl(~onAck=wrapOnAck(onAck), queue, content, _, channel);
