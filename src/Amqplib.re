open PromEx;

type connection;
type channel;

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

[@bs.send.pipe: connection] external createChannel: Js.Promise.t(channel) = "";

[@bs.send.pipe: channel]
external assertExchange: (string, string) => Js.Promise.t(Js.Json.t) = "";
let assertExchange = (exchange, type_, channel) =>
    assertExchange(exchange, type_, channel)
    |> map(exchangeInfo_decode)
    |> map(Belt.Result.getExn);

[@bs.send.pipe: channel]
external assertQueue: (~queue: string=?) => Js.Promise.t(Js.Json.t) = "";
let assertQueue = (~queue=?, channel) =>
    assertQueue(~queue?, channel)
    |> map(queueInfo_decode)
    |> map(Belt.Result.getExn);

[@bs.send.pipe: channel] external checkQueue: (string) => Js.Promise.t(Js.Json.t) = "";
let checkQueue = (queue, channel) =>
    checkQueue(queue, channel)
    |> map(queueInfo_decode)
    |> map(Belt.Result.getExn);

[@bs.send.pipe: channel]
external bindQueue:
    (~queue: string, ~exchange: string, ~key: string) => Js.Promise.t(unit) = "";

type consumeOptions;
[@bs.obj] external consumeOptions: (~noAck: bool=?, unit) => consumeOptions = "";

[@bs.send.pipe: channel]
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

[@bs.send.pipe: channel]
external publish:
    (string, string, Node.Buffer.t, publishOptions) => bool = "";
let publish = (~correlationId=?, ~replyTo=?, exchange, key, content) =>
    publishOptions(~correlationId?, ~replyTo?, ())
    |> publish(exchange, key, content);

[@bs.send.pipe: channel]
external sendToQueue: (string, Node.Buffer.t, publishOptions) => bool = "";
let sendToQueue = (~correlationId=?, ~replyTo=?, queue, content) =>
    publishOptions(~correlationId?, ~replyTo?, ())
    |> sendToQueue(queue, content);

[@bs.send.pipe: channel] external prefetch: int => unit = "";
