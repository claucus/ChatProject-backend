const config_module = require("./config")
const Redis = require("ioredis")

const RedisCli = new Redis({
    host:config_module.redis_host,
    port:config_module.redis_port,
    //password:config_module.redis_password
})

RedisCli.on("error",function(error){
    console.log("RedisCli connect error",error);
    RedisCli.quit();
})


async function GetRedis(key){
    try{
        const result = await RedisCli.get(key);
        if (result === null){
            console.log('result','<'+result+'>','this key cannot be find...')
            return null
        }

        console.log('result','<'+result+'>',"Get key success")
        return result
    }catch(error){
        console.log("get redis error, error is ",error);
        return null
    }
}

async function SetRedisExpire(key,value,exptime){
    try{
        await RedisCli.set(key,value)
        await RedisCli.expire(key,exptime)
        return true;
    }catch(error){
        console.log("Set Redis Expire error, error is ",error)
        return false;
    }
}

async function QueryRedis(key){
    try{
        const result = await RedisCli.exists(key)
        if (result === 0){
            console.log('result','<'+result+'>',"this key is null")
            return null
        }
        console.log('result','<'+result+'>',"with this value")
        return result
    }catch(error){
        console.log("Query Redis error, error is ",error)
        return null
    }
}


function Quit(){
    RedisCli.quit();
}

module.exports = {GetRedis,SetRedisExpire,QueryRedis,Quit}