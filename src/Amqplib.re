open PromEx;

type connection;
type channel;

[@decco]
type message = {
    content: [@decco.codec Decco.Codecs.magic] Node.Buffer.t,
};

[@decco]
type queue = {
    queue: string,
    messageCount: int,
    consumerCount: int,
};

[@bs.module "amqplib"] external connect: string => Js.Promise.t(connection) = "";

[@bs.send.pipe: connection] external close: Js.Promise.t(unit) = "";

[@bs.send.pipe: connection] external createChannel: Js.Promise.t(channel) = "";

[@bs.send.pipe: channel]
external assertQueue: (~queue: string=?) => Js.Promise.t(Js.Json.t) = "";
let assertQueue = (~queue=?, channel) =>
    assertQueue(~queue?, channel)
    |> map(queue_decode)
    |> map(Belt.Result.getExn);

[@bs.send.pipe: channel] external checkQueue: (string) => Js.Promise.t(Js.Json.t) = "";
let checkQueue = (queue, channel) =>
    checkQueue(queue, channel)
    |> map(queue_decode)
    |> map(Belt.Result.getExn);

[@bs.send.pipe: channel]
external consume: (string, Js.Json.t => unit) => Js.Promise.t(unit) = "";
let consume = (queue, cb) =>
    consume(queue, json =>
        message_decode(json)
        |> Belt.Result.getExn
        |> cb
    );

[@bs.send.pipe: channel]
external sendToQueue: (string, Node.Buffer.t) => Js.Promise.t(bool) = "";

[@bs.send.pipe: channel] external prefetch: int => unit = "";
