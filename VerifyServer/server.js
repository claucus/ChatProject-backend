const grpc = require("@grpc/grpc-js")
const message_proto = require("./proto")
const const_module = require("./const")
const {v4:uuidv4} = require("uuid")
const email_module = require("./email")
const redis_module = require("./redis")


async function GetVerifyCode(call,callback){
    console.log("email is ",call.request.eamil)
    try{
        let query_result = await redis_module.GetRedis(const_module.code_prefix+call.request.email);
        console.log("query result is ",query_result)
        let unique_id = query_result
        if (query_result === null){
            unique_id = uuidv4();

            let b_result = await redis_module.SetRedisExpire(const_module.code_prefix+call.request.email,unique_id,600)
            if (!b_result){
                callback(null,{
                    email:call.request.email,
                    error:const_module.Errors.REDIS_ERROR
                });
                return ;
            }
        }
        console.log("uniqueID is:",unique_id)
        let text_str = `校验码为:\n${unique_id}\n请在十分钟内完成注册。\n`

        let mailOptions = {
            from:"schwarzgg@163.com",
            to:call.request.email,
            subject:"校验码",
            text:text_str,
        }

        let send_res = await email_module.SendMail(mailOptions);
        console.log("send res is ",send_res)

        callback(null,{
            email:call.request.email,
            error:const_module.Errors.SUCCESS
        })

    }catch(error){
        console.log("catch error is ",error)

        callback(null,{
            email:call.request.email,
            error:const_module.Errors.EXCEPTION
        });
    }
}

function main(){
    var server = new grpc.Server()
    server.addService(message_proto.VerifyService.service,{
        GetVerifyCode:GetVerifyCode
    })
    server.bindAsync("127.0.0.1:50051",grpc.ServerCredentials.createInsecure(),()=>{
        console.log("gRPC Server Started Successfully!")
    })
}

main()
