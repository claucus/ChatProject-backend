const path = require("path")
const grpc = require("@grpc/grpc-js")
const protoLoader = require("@grpc/proto-loader")

const PROTO_PATH = path.join(__dirname,"verify.proto")

const packageDefinition = protoLoader.loadSync(PROTO_PATH,{keepCase:true,longs:String,enums:String,defaults:true,oneofs:true})

const protoDescriptor = grpc.loadPackageDefinition(packageDefinition);

const verify_proto = protoDescriptor.verify

module.exports = verify_proto