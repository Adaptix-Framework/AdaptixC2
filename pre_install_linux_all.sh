sudo apt update

# server
sudo apt install mingw-w64 make -y
wget https://go.dev/dl/go1.25.4.linux-amd64.tar.gz -O /tmp/go1.25.4.linux-amd64.tar.gz
sudo rm -rf /usr/local/go /usr/local/bin/go
sudo tar -C /usr/local -xzf /tmp/go1.25.4.linux-amd64.tar.gz
sudo ln -s /usr/local/go/bin/go /usr/local/bin/go

# for windows 7 support by gopher agent
git clone https://github.com/Adaptix-Framework/go-win7 /tmp/go-win7
sudo mv /tmp/go-win7 /usr/lib/

#client
sudo apt install gcc g++ build-essential make cmake mingw-w64 g++-mingw-w64 libssl-dev qt6-base-dev qt6-base-private-dev libxkbcommon-dev qt6-websockets-dev qt6-declarative-dev -y
