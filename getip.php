<?php
date_default_timezone_set('UTC');

$port = 8198;

$sock_srv = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);

socket_set_option($sock_srv, SOL_SOCKET, SO_REUSEADDR, 1);

if (!socket_bind($sock_srv, 0, $port)) {
	die('Bind Port '.$port ." Failed\n");
}

if (!socket_listen($sock_srv,5)) {
	die('Socket listen Failed On Port '.$port ."\n");
}

$clients = array($sock_srv);

echo "IPHelper Service running on port ".$port ." ...\n";

while(true) {
	$readSet = $clients;
	$writeSet = null;
	$expectSet = null;
	
	$number = socket_select($readSet, $writeSet, $expectSet, 10);
	
	if($number === False) {
		echo "Select error:".socket_strerror(socket_last_error()) ."\n";
		continue;
	}
	else if($number === 0) {
		//echo "Socket select nothing\n";
		continue;
	}
	//echo count($readSet).':'.count($writeSet).':'.count($expectSet)."\n";
	
	if(in_array($sock_srv, $readSet)) {
		$clients[] = $newsock = socket_accept($sock_srv);
				
		if (! @socket_getpeername($newsock, $ip) ) {
			echo "socket_getpeername Failed\n";
			$ip = '0.0.0.0';
		}
		echo "New client $ip arrived!\n";
		$key = array_search($sock_srv, $readSet);
		unset($readSet[$key]);
	}
	
	foreach ($readSet as $read_sock) {
		$data = @socket_read($read_sock, 1024);
		if($data === false || empty($data) ) {
			$key = array_search($read_sock, $clients);
			if (! @socket_getpeername($clients[$key], $ip)) {
				echo "socket_getpeername Failed\n";
				$ip = '0.0.0.0';
			}
			socket_close($read_sock);
			
			unset($clients[$key]);
			echo "client $ip disconnected\n";
			continue;
		}
		
		if (! @socket_getpeername($read_sock, $clientIp)) {
			//Never arrive here!
			$clientIp = '0.0.0.0';
		}
				
		$response = Array();
		
		$response['ip'] = $clientIp;
		
		$data = trim($data);
		
		if(!empty($data)) {
			echo 'Client:'.$clientIp ." login-data: ". $data."\n";
		}
		else {
			echo "nothing to read\n";
		}
		
		$responseS = json_encode($response,JSON_PRETTY_PRINT|JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE);
		
		socket_write($read_sock, $responseS);
		//wait for reponse sending		
		Sleep(1);
		
		$key = array_search($read_sock, $clients);
		socket_close($read_sock);
		unset($clients[$key]);
	}
}

socket_close($sock_srv);

