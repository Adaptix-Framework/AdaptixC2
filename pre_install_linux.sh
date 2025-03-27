sudo apt update

# server
sudo apt install golang-1.23 mingw-w64 make -y
sudo ln -s /usr/lib/go-1.23/bin/go /usr/local/bin/go

#client
sudo apt install make cmake libssl-dev qt6-base-dev qt6-websockets-dev -y
