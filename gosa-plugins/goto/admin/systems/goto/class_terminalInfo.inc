<?php

class terminfo extends plugin
{
    /* Generic terminal attributes */
    var $ghCpuType= "-";
    var $ghMemSize= "-";
    var $macAddress= "-";
    var $ghUsbSupport= "-";
    var $ghNetNic= array();
    var $ghIdeDev= array();
    var $ghScsiDev= array();
    var $ghGfxAdapter= "-";
    var $ghSoundAdapter= "-";
    var $ghInventoryNumber= "-";
    var $gotoLastUser= "-";
    var $gotoFloppyEnable= "";
    var $gotoCdromEnable= "";

    /* Needed values and lists */
    var $base= "";
    var $cn= "";
    var $view_logged = FALSE;

    /* attribute list for save action */
    var $ignore_account= TRUE;
    var $attributes= array("cn", "gotoMode", "gotoTerminalPath", "gotoFloppyEnable",
            "gotoCdromEnable", "ghInventoryNumber",
            "gotoSwapServer", "gotoSyslogServer", "gotoNtpServer",
            "ghCpuType", "ghMemSize", "macAddress", "ghUsbSupport",
            "ghGfxAdapter", "ghSoundAdapter", "gotoLastUser");
    var $objectclasses= array("GOhard");

    function terminfo (&$config, $dn= NULL, $parent= NULL)
    {
        plugin::plugin ($config,$dn);

        /* Read arrays */
        foreach (array("ghNetNic", "ghIdeDev", "ghScsiDev") as $val){
            if (!isset($this->attrs[$val])){
                continue;
            }
            for ($i= 0; $i<$this->attrs[$val]['count']; $i++){
                array_push($this->$val, $this->attrs[$val][$i]);
            }
        }

        /* Fix USB entry */
        if ($this->ghUsbSupport == "true"){
            $this->ghUsbSupport= _("present");
        }
    }


    function execute()
    {
        /* Call parent execute */
        plugin::execute();

        if($this->is_account && !$this->view_logged){
            $this->view_logged = TRUE;
            new log("view","terminal/".get_class($this),$this->dn);
        }

        /* Do we represent a valid terminal? */
        if (!$this->is_account && $this->parent === NULL){
            return("<img alt=\"\" src=\"images/small-error.png\" align=middle>&nbsp;<b>".
                    msgPool::noValidExtension(_("terminal"))."</b>");
        }

        $smarty= get_smarty();
        $display= "";
        $smarty->assign("ACL",TRUE);
        if(!preg_match("/r/",$this->getacl(""))){
            $smarty->assign("ACL",FALSE);
        }elseif(!is_callable("snmpget")){

            $smarty->assign("load", progressbar(0,100,15,true));
            $smarty->assign("mem", progressbar(0,100,15,true));
            $smarty->assign("swap", progressbar(0,100,15,true));
            foreach(array("uptime", "sshd", "X", "saned", "artsd", "cupsd","status","ghNetNic", "ghIdeDev", "ghScsiDev","FloppyDevice", "CdromDevice","active") as $val){
                $smarty->assign("$val", "<i>"._("unknown status, SNMP support missing")."</i>");
            }


            $display ="";
        }else
            /* Default entry? */
            if ($this->cn == "default"){
                $display= "<div style='height:150px;'><br><b>";
                $display.= _("This is a virtual terminal which has no properties to show here.");
                $display.= "</b></div>";
            } else {

                /* Get template object */

                /* Prevent undefined variable .... */ 
                $smarty->assign("load", progressbar(0,100,15,true));
                $smarty->assign("mem", progressbar(0,100,15,true));
                $smarty->assign("swap", progressbar(0,100,15,true));

                /* Check if terminal is online */
                if (gosaSupportDaemon::ping($this->macAddress)){
                    $smarty->assign("status", _("on-line"));
                    $smarty->assign("active", "true");

                    /* Fill data if we have snmp */
                    $host= $this->cn;

                    /* Use 'goto' as snmp community or the configured value from the config */
                    $community= 'goto';
                    $str= $this->config->get_cfg_value("terminfo", "snmpCommunity");
                    if(!empty($str)){
                        $community = $str;
                    }

                    /* Get memory informations */
                    if(!is_callable("snmpget")){
                        $MemFree = false;
                    }else{
                        $MemFree= @snmpget($host, $community, "UCD-SNMP-MIB::memory.memAvailReal.0");
                    }
                    if ($MemFree != FALSE){
                        $MemFree= preg_replace('/^.*[=:] ([0-9.]+)$/', '\\1', $MemFree);
                        $MemTotal= @snmpget($host, $community, "UCD-SNMP-MIB::memory.memTotalReal.0");
                        $MemTotal= preg_replace('/^.*[=:] ([0-9.]+)$/', '\\1', $MemTotal);
                        if ($MemTotal != 0){
                            $smarty->assign("mem",progressbar( (int)(($MemTotal - $MemFree)*100/$MemTotal),100,15,true));
                            ;
                        }
                        $SwapFree= @snmpget($host, $community, "UCD-SNMP-MIB::memory.memAvailSwap.0");
                        $SwapFree= preg_replace('/^.*[=:] ([0-9.]+)$/', '\\1', $SwapFree);
                        $SwapTotal= @snmpget($host, $community, "UCD-SNMP-MIB::memory.memTotalSwap.0");
                        $SwapTotal= preg_replace('/^.*[=:] ([0-9.]+)$/', '\\1', $SwapTotal);
                        if ($SwapTotal != 0){
#$smarty->assign("swap", (int)(($SwapTotal - $SwapFree)*100/$SwapTotal));
                            $smarty->assign("swap", progressbar(0,100,15,true));
                        }

                        /* Get system uptime */
                        $sysup= @snmpget($host, $community, "SNMPv2-MIB::sysUpTime.0");
                        $smarty->assign("uptime", preg_replace('/^.* ([0-9:]+)\..*$/', '\\1', $sysup));

                        /* Get system load */
                        $sysload= @snmpget($host, $community, "UCD-SNMP-MIB::laLoad.2");
                        $sysload= preg_replace('/^.*[=:] ([0-9.]+)$/', '\\1', $sysload);

                        $smarty->assign("load", progressbar($sysload*100,100,15,true));

                        /* Get status for key processes */
                        $processes= @snmpwalk($host, $community, "UCD-SNMP-MIB::prNames");
                        $check4= array("sshd", "cupsd", "artsd", "X", "saned");
                        foreach ($check4 as $pname){
                            $eflag= -1;
                            foreach ($processes as $key => $val){
                                $process= preg_replace('/^.*[:=] (.*)$/', '\\1', $val);
                                if ($process == $pname){
                                    $index= preg_replace('/^.*\.([0-9]+) [:=] .*$/', '\\1', $val);
                                    $res= @snmpget($host, $community, "UCD-SNMP-MIB::prErrorFlag.$index");
                                    $eflag= preg_replace('/^.*[:=] /', '', $res);
                                    break;
                                }
                            }
                            switch ($eflag){
                                case 0:
                                    $smarty->assign("$pname", "<img alt=\""._("running")."\" src=\"images/true.png\">");
                                    break;
                                case 1:
                                    $smarty->assign("$pname", "<img alt=\""._("not running")."\" src=\"images/false.png\">");
                                    break;
                                default:
                                    $smarty->assign("$pname", _("not defined"));
                            }
                        }
                    } else {
                        foreach(array("uptime", "sshd", "X", "saned", "artsd", "cupsd") as $val){
                            $smarty->assign("$val", "<i>"._("unknown status")."</i>");
                        }
                    }
                    /* Check for mounted partitions (show max 8 partitions) */
                    $partitions= "";
                    for ($n= 1; $n<9; $n++){
                        $device= @snmpget($host, $community, "UCD-SNMP-MIB::dskDevice.$n");
                        if ($device == ""){
                            break;
                        }
                        $device= preg_replace('/^STRING: */', '', $device);
                        $usage= @snmpget($host, $community, "UCD-SNMP-MIB::dskPercent.$n");
                        $usage= preg_replace('/^INTEGER: */', '', $usage);
                        $partitions.= "<tr><td><b>$device</b></td><td>".progressbar($usage,100,16,true)."</td></tr>\n";
                    }
                    $smarty->assign("partitions", $partitions);
                } else {
                    $smarty->assign("status", _("off-line"));
                    $smarty->assign("active", "false");
                }

                /* Set floppy and cdrom status */
                foreach(array("Floppy", "Cdrom") as $val){
                    $name= "goto".$val."Enable";
                    if ($this->$name == "YES"){
                        $status= _("present");
                    } else {
                        $status= "-";
                    }
                    $smarty->assign($val."Device", $status);
                }

                /* Show main page */
                foreach(array("ghNetNic", "ghIdeDev", "ghScsiDev") as $val){
                    if (!count($this->$val)){
                        $this->$val= "-";
                    }
                    $smarty->assign($val, $this->$val);
                }
            }
        foreach(array("ghCpuType", "ghMemSize", "macAddress", "ghUsbSupport",
                    "ghGfxAdapter", "ghSoundAdapter", "gotoLastUser", "ghInventoryNumber") as $val){

            $smarty->assign($val, $this->$val);
        }
        $display= $smarty->fetch (get_template_path('info.tpl', TRUE, dirname(__FILE__)));

        return ($display);
    }

    function remove_from_parent()
    {
    }


    /* Save data to object */
    function save_object()
    {
        plugin::save_object();
    }


    /* Save to LDAP */
    function save()
    {
    }


    /* Return plugin informations for acl handling */
    static function plInfo()
    {
        return (array(
                    "plShortName"   => _("System info"),
                    "plDescription" => _("System informations"),
                    "plSelfModify"  => FALSE,
                    "plDepends"     => array(),
                    "plPriority"    => 33,
                    "plSection"     => array("administration"),
                    "plCategory"    => array("workstation","server","terminal"),

                    "plProperties" => array(
                        array(
                            "name"          => "snmpCommunity",
                            "type"          => "string",
                            "default"       => "",
                            "description"   => _("Name of GOto SNMP community."),
                            "check"         => "gosaProperty::isString",
                            "migrate"       => "",
                            "group"         => "plugin",
                            "mandatory"     => FALSE
                            )
                        ),

                    "plProvidedAcls"=> array()
                        ));
    }
}

// vim:tabstop=2:expandtab:shiftwidth=2:filetype=php:syntax:ruler:
?>
