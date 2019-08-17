open Jest;
open Expect;
open Js.Promise;
open PromEx;
open Amqplib;

let connection = connect("amqp://guest:guest@/");

let channel = () =>
    connection |> then_(createChannel);

let confirmChannel = () =>
    connection |> then_(createConfirmChannel);

let randomName = (prefix) =>
    prefix ++ (Js.Math.random() |> Js.Float.toString);

testPromise("assertExchange", () => {
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
    testPromise("unnamed", () => {
        channel()
        |> then_(channel =>
            assertQueue(channel)
            |> then_(({queue}) => checkQueue(queue, channel))
        )
        |> map(({queue}) => Js.String.length(queue))
        |> map(expect)
        |> map(toBeGreaterThan(0))
    });

    testPromise("named", () => {
        let name = randomName("named");

        channel()
        |> then_(assertQueue(~queue=name))
        |> map(({queue}) => queue)
        |> map(expect)
        |> map(toBe(name))
    });
});

testAsync(~timeout=250, "send/consume", done_ => {
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

testAsync(~timeout=250, "publish", done_ => {
    channel()
    |> then_(channel => {
        let exchange = randomName("exchange");
        let key = randomName("key");
        let msg = randomName("sup");

        assertExchange(exchange, "fanout", channel)
        |> then_(_ => assertQueue(channel))
        |> then_(({queue}) =>
            bindQueue(~queue, ~exchange, ~key, channel)
            |> then_(_ =>
                channel |> consume(queue, ({content}) =>
                    Node.Buffer.toString(content)
                    |> expect |> toBe(msg)
                    |> done_
                )
            )
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

testPromise("reply-to", () => {
    Js.Promise.make((~resolve, ~reject as _) =>
        channel()
        |> then_(channel => {
            let exchange = randomName("exchange");
            let key = randomName("key");
            let msg = randomName("howdy");

            assertExchange(exchange, "direct", channel)
            |> then_(_ => assertQueue(channel))
            |> then_(({queue}) =>
                bindQueue(~queue, ~exchange, ~key, channel)
                |> then_(_ =>
                    channel
                    |> consume(queue,
                        ({content, properties: {replyTo, correlationId}}) =>
                            Belt.Option.getExn(replyTo)
                            |> sendToQueue(~correlationId?, _, content, channel)
                            |> ignore
                    )
                )
            )
            |> then_(_ => {
                let replyTo = "amq.rabbitmq.reply-to";
                let correlationId = randomName("hardcore");

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
        |> ignore
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
        testAsync(~timeout=250, "success", done_ =>
            confirmChannel()
            |> flatAmend(assertExchange(randomName("exch"), "fanout"))
            |> map(((channel, {exchange})) => {
                (err => expect(err) |> toBe(None) |> done_)
                |> publishAck(exchange, "key", Node.Buffer.fromString(""), _, channel)
            })
            |> ignore
        );

        testAsync(~timeout=250, "failure", done_ =>
            confirmChannel()
            |> flatAmend(assertExchange(randomName("exch"), "fanout"))
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

afterAllPromise(() => connection |> then_(close));
