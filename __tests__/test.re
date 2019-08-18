open Jest;
open Expect;
open Js.Promise;
open PromEx;
open Amqplib;

let timeout = 250;

let connection = connect("amqp://guest:guest@/");

let channel = () =>
    connection |> then_(createChannel);

let confirmChannel = () =>
    connection |> then_(createConfirmChannel);

let randomName = (prefix) =>
    prefix ++ (Js.Math.random() |> Js.Float.toString);

let exchange = (~type_="fanout", channel) =>
    channel |> flatAmend(assertExchange(randomName("exchange"), type_));

let queue = (channelExchange) =>
    channelExchange
    |> flatAmend(((channel, {exchange})) => {
        let key = randomName("key");
        assertQueue(channel)
        |> flatAmend(({queue}) =>
            bindQueue(~queue, ~exchange, ~key, channel)
            |> map(_ => key)
        )
    });

let catchAndReject = (reject, p) =>
    p
    |> catch(err => {
        reject(. Obj.magic(err));
        failwith("")
    })
    |> ignore;

testPromise(~timeout, "assertExchange", () => {
    channel()
    |> then_(channel => {
        let exchange = randomName("exchange");
        assertExchange(exchange, "fanout", channel)
        |> map(({exchange}) => exchange)
        |> map(expect)
        |> map(toBe(exchange))
    })
});

describe("assertQueue", () => {
    testPromise(~timeout, "unnamed", () => {
        channel()
        |> then_(channel =>
            assertQueue(channel)
            |> then_(({queue}) => checkQueue(queue, channel))
        )
        |> map(({queue}) => Js.String.length(queue))
        |> map(expect)
        |> map(toBeGreaterThan(0))
    });

    testPromise(~timeout, "named", () => {
        let name = randomName("named");

        channel()
        |> then_(assertQueue(~queue=name))
        |> map(({queue}) => queue)
        |> map(expect)
        |> map(toBe(name))
    });
});

testAsync(~timeout, "send/consume", done_ => {
    channel()
    |> then_(channel => {
        let queue = randomName("send-consume");
        let msg = randomName("hey hey");

        channel
        |> assertQueue(~queue)
        |> then_(_ =>
            channel
            |> consume(queue, ({content}) =>
                Node.Buffer.toString(content)
                |> expect |> toBe(msg)
                |> done_
            )
        )
        |> map(_ => sendToQueue(queue, Node.Buffer.fromString(msg), channel))
    })
    |> catch(e => {
        expect(Js.String.make(e))
        |> toBe("")
        |> done_;
        reject(Obj.magic(e));
    })
    |> ignore
});

testAsync(~timeout, "publish", done_ => {
    channel()
    |> exchange
    |> queue
    |> then_((((channel, {exchange}), ({queue}, key))) => {
        let msg = randomName("sup");

        channel |> consume(queue, ({content}) =>
            Node.Buffer.toString(content)
            |> expect |> toBe(msg)
            |> done_
        )
        |> map(_ => publish(exchange, key, Node.Buffer.fromString(msg), channel))
    })
    |> catch(e => {
        expect(Js.String.make(e))
        |> toBe("")
        |> done_;
        reject(Obj.magic(e));
    })
    |> ignore
});

testPromise(~timeout, "reply-to", () => {
    Js.Promise.make((~resolve, ~reject) =>
        channel()
        |> exchange(~type_="direct")
        |> queue
        |> then_((((channel, {exchange}), ({queue}, key))) => {
            channel
            |> consume(queue,
                ({content, properties: {replyTo, correlationId}}) =>
                    Belt.Option.getExn(replyTo)
                    |> sendToQueue(~correlationId?, _, content, channel)
                    |> ignore
            )
            |> then_(_ => {
                let replyTo = "amq.rabbitmq.reply-to";
                let correlationId = randomName("hardcore");
                let msg = randomName("howdy");

                channel
                |> consume(~noAck=true, replyTo,
                    ({content, properties: {correlationId: actualCorrelationId}}) =>
                        (Node.Buffer.toString(content), actualCorrelationId)
                        |> expect |> toEqual((msg, Some(correlationId)))
                        |> resolve(. _)
                )
                |> map(_ => publish(
                    ~correlationId, ~replyTo,
                    exchange, key, Node.Buffer.fromString(msg), channel
                ))
            })
        })
        |> catchAndReject(reject)
    )
});

describe("confirm", () => {
    describe("sendToQueue", () => {
        testAsync(~timeout=10000, "success", done_ =>
            confirmChannel()
            |> flatAmend(assertQueue)
            |> map(((channel, {queue})) => {
                (err => expect(err) |> toBe(None) |> done_)
                |> sendToQueueAck(queue, Node.Buffer.fromString(""), _, channel)
            })
            |> ignore
        );

        testAsync(~timeout=10000, "failure", done_ =>
            confirmChannel()
            |> flatAmend(assertQueue)
            |> map(((channel, {queue})) => {
                closeChannel(channel);

                (err =>
                    Belt.Option.flatMap(err, Js.Exn.message)
                    |> expect |> toBe(Some("channel closed")) |> done_
                )
                |> sendToQueueAck(queue, Node.Buffer.fromString(""), _, channel)
            })
            |> map(_ => ())
            |> catch(_ => resolve())
            |> ignore
        );
    });

    describe("publish", () => {
        testAsync(~timeout, "success", done_ =>
            confirmChannel()
            |> exchange
            |> map(((channel, {exchange})) => {
                (err => expect(err) |> toBe(None) |> done_)
                |> publishAck(exchange, "key", Node.Buffer.fromString(""), _, channel)
            })
            |> ignore
        );

        testAsync(~timeout, "failure", done_ =>
            confirmChannel()
            |> exchange
            |> map(((channel, {exchange})) => {
                closeChannel(channel);

                (err =>
                    Belt.Option.flatMap(err, Js.Exn.message)
                    |> expect |> toBe(Some("channel closed")) |> done_
                )
                |> publishAck(exchange, "key", Node.Buffer.fromString(""), _, channel)
            })
            |> map(_ => ())
            |> catch(_ => resolve())
            |> ignore
        );
    });
});

testPromise("ack/nack", () =>
    Js.Promise.make((~resolve, ~reject) =>
        channel()
        |> exchange
        |> queue
        |> then_((((channel, {exchange}), ({queue}, key))) => {
            let attempts = ref(0);

            // Keep track of the # of attempts, and don't ack until the 3rd
            // If we check `attempts` after a short period of time, it should === 3
            channel |> consume(queue, ({raw}) => {
                incr(attempts)

                attempts^ === 3
                    ? ack(raw, channel)
                    : nack(raw, channel);
            })
            |> map(_ => publish(exchange, key, Node.Buffer.fromString(""), channel))
            |> map(_ =>
                250 |> Js.Global.setTimeout(() =>
                    expect(attempts^) |> toBe(3)
                    |> resolve(. _)
                )
            )
        })
        |> catchAndReject(reject)
    )
);

afterAllPromise(() => connection |> then_(close));
