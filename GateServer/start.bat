@echo off

set PROTOC=E:\vcpkg\installed\x64-windows\tools\protobuf\protoc.exe
set PLUGIN=E:\vcpkg\installed\x64-windows\tools\grpc\grpc_cpp_plugin.exe
set FILE=user.proto

echo Generating gRPC code...
%PROTOC% -I=. --grpc_out=. --plugin=protoc-gen-grpc=%PLUGIN% %FILE%

echo Generating C++ code...
%PROTOC% -I=. --cpp_out=. %FILE%

echo Done.