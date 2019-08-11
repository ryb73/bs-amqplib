open Jest;
open Expect;
open Js.Promise;
open PromEx;
open Amqplib;

let connection = connect("amqp://guest:guest@/");

let channel = () =>
    connection |> then_(createChannel);

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
        let name = "named" ++ (Js.Math.random() |> Js.Float.toString);

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
        let queue = "send-consume" ++ (Js.Math.random() |> Js.Float.toString);
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
    |> ignore
});

afterAllPromise(() => connection |> then_(close));
