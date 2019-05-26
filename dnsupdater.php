<?php
date_default_timezone_set('UTC');

set_time_limit(0);
$iphelper_addr = 'www.iavcast.com';
$iphelper_port = '8198';
$AccessKeyId     = 'your AccessKeyId';
$AccessKeySecret = 'your AccessKeySecret';
$domain_list = Array('video.yourdomain.com','www.yourdomain.com','video.yourdomain.net','file.yourdomain.net');//

$show_log = False;
$old_gateway_ip = '';

$options = getopt("d:h:");
if (count($options) > 0) {
	if (!empty($options['d']))
		$show_log = True;
}


if (!function_exists('random_int')) {
	//php 5.x compatible
	function random_int($min,$max) {
		return mt_rand($min,$max);
	}
}

/**
 * Class AlicloudDNSUpdater
 */
class AlicloudDNSUpdater {
    /**
     * @var string
     */
    public $domainName;

    /**
     * @var string
     */
    public $rR;

    /**
     * @var string
     */
    public $type;

    /**
     * @var string
     */
    public $value;

    /**
     * @var string
     */
    public $accessKeyId;

    /**
     * @var string
     */
    public $accessKeySecret;

    /**
     * AlicloudUpdateRecord constructor.
     *
     * @param string $accessKeyId
     * @param string $accessKeySecret
     */
    function __construct(
        $accessKeyId,
        $accessKeySecret
    ) {
        $this->accessKeyId     = $accessKeyId;
        $this->accessKeySecret = $accessKeySecret;
    }

    /**
     * @param string $CanonicalQueryString
     * @return string
     */
    public function getSignature($CanonicalQueryString)
    {
        $HTTPMethod                  = 'GET';
        $slash                       = urlencode('/');
        $EncodedCanonicalQueryString = urlencode($CanonicalQueryString);
        $StringToSign                = "{$HTTPMethod}&{$slash}&{$EncodedCanonicalQueryString}";
        $StringToSign                = str_replace('%40', '%2540', $StringToSign);
        $HMAC                        = hash_hmac('sha1', $StringToSign, "{$this->accessKeySecret}&", true);

        return base64_encode($HMAC);
    }

    /**
     * @return string
     */
    public function getDate()
    {
        $timestamp = date('U');
        $date      = date('Y-m-d', $timestamp);
        $H         = date('H', $timestamp);
        $i         = date('i', $timestamp);
        $s         = date('s', $timestamp);

        return "{$date}T{$H}%3A{$i}%3A{$s}";
    }

    /**
     * @return string
     * @throws Exception
     */
    public function getRecordId()
    {
        $queries = [
            'AccessKeyId' => $this->accessKeyId,
            'Action' => 'DescribeDomainRecords',
            'DomainName' => $this->domainName,
            'Format' => 'json',
            'SignatureMethod' => 'HMAC-SHA1',
            'SignatureNonce' => random_int(1000000000, 9999999999),
            'SignatureVersion' => '1.0',
            'Timestamp' => $this->getDate(),
            'Version' => '2015-01-09'
        ];

        $response = $this->doRequest($queries);

		if (!isset($response['DomainRecords'])) {
			return '';
		}
		
        $recordList = $response['DomainRecords']['Record'];

        $RR = null;
        foreach ($recordList as $key => $record) {
            if ($this->rR === $record['RR']) {
                $RR = $record;
            }
        }

        if ($RR === null) {
            //die('RR ' . $this->rR . ' not found.');
			return '';
        }

        return $RR['RecordId'];
    }

    /**
     * @param string $domainName
     */
    public function setDomainName($domainName)
    {
        $this->domainName = $domainName;
    }

    /**
     * @param string $value
     */
    public function setValue($value)
    {
        $this->value = $value;
    }

    /**
     * @param string $rR
     */
    public function setRR($rR)
    {
        $this->rR = $rR;
    }

    /**
     * @param string $recordId
     */
    public function setRecordId($recordId)
    {
        $this->recordId = $recordId;
    }

    /**
     * @param string $type
     */
    public function setRecordType($type)
    {
        $this->type = $type;
    }

    /**
     * @param array $queries
     * @return array
     */
    public function doRequest($queries)
    {
        $CanonicalQueryString = '';
        $i                    = 0;
        foreach ($queries as $param => $query) {
            $CanonicalQueryString .= $i === 0 ? null : '&';
            $CanonicalQueryString .= "{$param}={$query}";
            $i++;
        }

        $signature  = $this->getSignature($CanonicalQueryString);
        $requestUrl = "http://dns.aliyuncs.com/?{$CanonicalQueryString}&Signature=" . urlencode($signature);
        $response   = file_get_contents($requestUrl, false, stream_context_create([
            'http' => [
                'ignore_errors' => true
            ]
        ]));

        return json_decode($response, true);
    }

    /**
     * @return array
     * @throws \Exception
     */
    public function sendRequest()
    {
		$RecordId = $this->getRecordId();
		if (empty($RecordId)) {
			return Array(
				'Code'=>'Error',
				'Message'=>$this->domainName .' record not found'
			);
		}
		
        $queries = [
            'AccessKeyId' => $this->accessKeyId,
            'Action' => 'UpdateDomainRecord',
            'Format' => 'json',
            'RR' => $this->rR,
            'RecordId' => $RecordId,
            'SignatureMethod' => 'HMAC-SHA1',
            'SignatureNonce' => random_int(1000000000, 9999999999),
            'SignatureVersion' => '1.0',
            'Timestamp' => $this->getDate(),
            'Type' => $this->type,
            'Value' => $this->value,
            'Version' => '2015-01-09'
        ];

        return $this->doRequest($queries);
    }
	
	public function sendAddRequest()
    {
        $queries = [
            'AccessKeyId' => $this->accessKeyId,
            'Action' => 'AddDomainRecord',
            'Format' => 'json',
            'RR' => $this->rR,
			'Type' => $this->type,
            'Value' => $this->value,
            'DomainName' => $this->domainName,
            'SignatureMethod' => 'HMAC-SHA1',
            'SignatureNonce' => random_int(1000000000, 9999999999),
            'SignatureVersion' => '1.0',
            'Timestamp' => $this->getDate(),
            'Version' => '2015-01-09'
        ];

        return $this->doRequest($queries);
    }
}

while(true) {
	$client = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);

	$result = @socket_connect($client, $iphelper_addr, $iphelper_port);
	if (!$result) {
		if ($show_log) {
			echo "socket_connect() failed: reason: " . socket_strerror(socket_last_error($client)) . "\n";
		}
		socket_close($client);
	}
	else {
		$login_info = Array(
			'server'=>'server 1',
			'time'=>time()
		);
		
		socket_write($client, json_encode($login_info));
		
		$response = @socket_read($client, 1024);
		
		if ($response !== False && !empty($response) ) {
			$info = json_decode($response,True);
			if (is_array($info) && isset($info['ip'])) {
				if ($old_gateway_ip != $info['ip']) {
					if ($show_log) {
						echo date('Y-m-d H:i:s'). " do refresh dns ip :".$info['ip']."\n";
					}

					$updater         = new AlicloudDNSUpdater($AccessKeyId, $AccessKeySecret);
																	
					foreach($domain_list as $domain) {
						$dotpos = strpos($domain,'.');
						
						if ($dotpos !== False) {
							$recoreName = substr($domain,0,$dotpos);
							$domainName = substr($domain,$dotpos + 1);
							if ($show_log) {
								echo 'Update DNS Record:'.$recoreName.'.'.$domainName .' -> '. $info['ip'] ."...\n";
							}
							
							$updater->setDomainName($domainName);
							$updater->setRecordType('A');
							$updater->setRR($recoreName);
							$updater->setValue($info['ip']);

							$result = $updater->sendRequest();
							if ($show_log) {
								print_r($result);
								
								echo "\n";
							}
						}
					}
										
					$old_gateway_ip = $info['ip'];
				}
				else {
					if ($show_log) {
						echo "IP:{$old_gateway_ip} keep!\n";
					}
				}
			}
		}
			
		socket_close($client);
	}
		
	Sleep(10);
}


