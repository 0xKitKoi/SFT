This is a quick and simple way to transfer files between two computers.
On the computer you want to recieve a file, run sft and type 'get'. It will ask you what IP and PORT to listen on.
Make sure the port you supply is forwarded if you are transfering files over WAN.
Then, on the computer with the file, run sft and type 'send'. It will ask you what file you want to transfer. 
Then it will ask you for the IP and PORT to attempt to send the file to. 

As of writing this, it only supports sending one file at a time. If I care about it, I'll make it so you can send multiple.
I dont reccomend typing the entire file path out, copy and pasting it sounds wack. If you drag and drop the file into the terminal window, 
it SHOULD paste the file path to the terminal, encased in quotation marks. I made sure to check for that and subtract them off. 
I made this because i was tired of using SCP to send a file to my raspberry pi.

TODO:
Send multiple files at the same time
dont close the server after a file has completed?
command line arguments (EX: ./sft.out -IP 192.168.0.000 -P 9999 -Server) / (EX: ./SFT.exe -Client -File C:/file.txt -IP 192.168.0.222 -P 9999)
get IP dynamically instead of hardcoding/asking for it.

