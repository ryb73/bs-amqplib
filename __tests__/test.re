open Jest;
open Expect;
open Js.Promise;
open PromEx;
open Amqplib;

let connection = connect("amqp://guest:guest@/");

let channel = () =>
    connection |> then_(createChannel);

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

testAsync("send/consume", done_ => {
    channel()
    |> then_(channel => {
        let queue = randomName("send-consume");
        let msg = "hey hey";

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
        |> then_(_ => sendToQueue(queue, Node.Buffer.fromString(msg), channel))
    })
    |> catch(e => {
        expect(Js.String.make(e))
        |> toBe("")
        |> done_;
        reject(Obj.magic(e));
    })
    |> ignore
});

testAsync("publish", done_ => {
    channel()
    |> then_(channel => {
        let exchange = randomName("exchange");
        let key = randomName("key");
        let msg = "sup";

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
        ) |> then_(_ => publish(exchange, key, Node.Buffer.fromString(msg), channel))
    })
    |> catch(e => {
        expect(Js.String.make(e))
        |> toBe("")
        |> done_;
        reject(Obj.magic(e));
    })
    |> ignore
});

afterAllPromise(() => connection |> then_(close));
