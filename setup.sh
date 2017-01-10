#!/usr/bin/env bash

apt-get update
apt-get install -y emacs
apt-get install -y git
apt-get install -y cmake
apt-get install -y make
apt-get install -y g++
apt-get install -y wget
apt-get install -y binutils-gold binutils-dev
apt-get install -y linux-tools-generic
apt-get install -y python-dev
apt-get install -y python-tk
wget https://bootstrap.pypa.io/get-pip.py
python ./get-pip.py
pip install matplotlib
pip install numpy
pip install pandas
pip install seaborn

if ! [ -L /var/www ]; then
    rm -rf /var/www
    ln -fs /vagrant /var/www
fi
