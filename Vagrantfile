# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure("2") do |config|
  # Every Vagrant development environment requires a box. You can search for
  # boxes at https://vagrantcloud.com/search.
  
  config.vm.box = "ubuntu/trusty64"
  
  # use shell and other provisioners as usual
  config.vm.provision :shell, path: "bootstrap.sh"
  
  config.vm.provider "virtualbox" do |v|
    v.memory = 10000
    v.cpus = 4
  end
  config.vm.provision "shell" do |s|
    s.inline = <<-SCRIPT
      # Change directory automatically on ssh login
      echo "export CXX=\"g++-5\" CC=\"gcc-5\"" >> /home/vagrant/.bashrc
      echo "cd    /vagrant" >> /home/vagrant/.bashrc
    SCRIPT
  end
end
