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
    channel
    |> flatAmend(assertExchange(~durable=false, _, randomName("exchange"), type_));

let queue = (channelExchange) =>
    channelExchange
    |> flatAmend(((channel, {exchange})) => {
        let key = randomName("key");
        assertQueue(~durable=false, channel)
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
        assertExchange(~durable=false, channel, exchange, "fanout")
        |> map(({exchange}) => exchange)
        |> map(expect)
        |> map(toBe(exchange))
    })
});

describe("assertQueue", () => {
    testPromise(~timeout, "unnamed", () => {
        channel()
        |> then_(channel =>
            assertQueue(~durable=false, channel)
            |> then_(({queue}) => checkQueue(channel, queue))
        )
        |> map(({queue}) => Js.String.length(queue))
        |> map(expect)
        |> map(toBeGreaterThan(0))
    });

    testPromise(~timeout, "named", () => {
        let name = randomName("named");

        channel()
        |> then_(assertQueue(~durable=false, ~queue=name))
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
        |> assertQueue(~durable=false, ~queue)
        |> then_(_ =>
            consume(channel, queue, ({content}) =>
                Node.Buffer.toString(content)
                |> expect |> toBe(msg)
                |> done_
            )
        )
        |> map(_ => sendToQueue(channel, queue, Node.Buffer.fromString(msg)))
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

        consume(channel, queue, ({content}) =>
            Node.Buffer.toString(content)
            |> expect |> toBe(msg)
            |> done_
        )
        |> map(_ => publish(channel, exchange, key, Node.Buffer.fromString(msg)))
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
            consume(channel, queue,
                ({content, properties: {replyTo, correlationId}}) =>
                    Belt.Option.getExn(replyTo)
                    |> sendToQueue(~correlationId?, channel, _, content)
                    |> ignore
            )
            |> then_(_ => {
                let replyTo = "amq.rabbitmq.reply-to";
                let correlationId = randomName("hardcore");
                let msg = randomName("howdy");

                consume(~noAck=true, channel, replyTo,
                    ({content, properties: {correlationId: actualCorrelationId}}) =>
                        (Node.Buffer.toString(content), actualCorrelationId)
                        |> expect |> toEqual((msg, Some(correlationId)))
                        |> resolve(. _)
                )
                |> map(_ => publish(
                    ~correlationId, ~replyTo,
                    channel, exchange, key, Node.Buffer.fromString(msg)
                ))
            })
        })
        |> catchAndReject(reject)
    )
});

describe("confirm", () => {
    describe("sendToQueue", () => {
        testAsync(~timeout, "success", done_ =>
            confirmChannel()
            |> flatAmend(assertQueue(~durable=false))
            |> map(((channel, {queue})) => {
                (err => expect(err) |> toBe(None) |> done_)
                |> sendToQueueAck(channel, queue, Node.Buffer.fromString(""))
            })
            |> ignore
        );

        testAsync(~timeout, "failure", done_ =>
            confirmChannel()
            |> flatAmend(assertQueue(~durable=false))
            |> map(((channel, {queue})) => {
                closeChannel(channel);

                (err =>
                    Belt.Option.flatMap(err, Js.Exn.message)
                    |> expect |> toBe(Some("channel closed")) |> done_
                )
                |> sendToQueueAck(channel, queue, Node.Buffer.fromString(""))
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
                |> publishAck(channel, exchange, "key", Node.Buffer.fromString(""))
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
                |> publishAck(channel, exchange, "key", Node.Buffer.fromString(""))
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
            consume(channel, queue, ({raw}) => {
                incr(attempts)

                attempts^ === 3
                    ? ack(channel, raw)
                    : nack(channel, raw);
            })
            |> map(_ => publish(channel, exchange, key, Node.Buffer.fromString("")))
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

testPromise("cancel", () =>
    channel()
    |> exchange
    |> queue
    |> then_((((channel, _), ({queue}, _))) =>
        consume(channel, queue, _ => ())
        |> map(({consumerTag}) => consumerTag)
        |> then_(cancel(channel))
        |> then_(_ => checkQueue(channel, queue))
        |> map(({consumerCount}) => consumerCount)
        |> map(expect) |> map(toBe(0))
    )
);

afterAllPromise(() => connection |> then_(close));
