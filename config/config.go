/* 
Copyright (c) 2012 Landeshauptstadt München
Author: Matthias S. Benkmann

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, 
MA  02110-1301, USA.
*/

// Manages all user-configurable flags and variables.
package config

import (
         "io"
         "os"
         "net"
         "fmt"
         "time"
         "bufio"
         "strings"
         "crypto/aes"
         
         "../util"
       )

// The initialization vector for the AES encryption of GOsa messages.
var InitializationVector = []byte(util.Md5sum("GONICUS GmbH")[0:aes.BlockSize])

// The keys used to address different gosa-si modules.
var ModuleKeys = []string{"dummy-key"}

// Maps a module name surrounded by brackets (such as "[ServerPackages]") to its key.
var ModuleKey = map[string]string{}

// The address to listen on. "127.0.0.1:<port>" listens only for connections from
// the local machine. ":<port>" allows connections from anywhere.
var ServerListenAddress = ":20081"

// IP address part of <source> element.
var IP = "127.0.0.1"

// The address sent in the <source> element.
var ServerSourceAddress = "127.0.0.1:20081"

// Where to send log messages (in addition to stderr).
var LogFilePath = "/var/log/go-susi.log"

// Path of the server config file.
var ServerConfigPath = "/etc/gosa-si/server.conf"

// Path to database of scheduled jobs.
var JobDBPath = "/var/lib/go-susi/jobdb.xml"

// Path to database of peer servers.
var ServerDBPath = "/var/lib/go-susi/serverdb.xml"

// host:port addresses of peer servers read from config file.
var PeerServers = []string{}

// This machine's hostname.
var Hostname = "localhost"

// This machine's domain name.
var Domain = "localdomain"

// The MAC address to send in the <macaddress> element.
var MAC = "01:02:03:04:05:06"

type InterfaceInfo struct {
  IP string   // IP address of this machine for this interface. If this string begins with "ERROR!", then it could not be determined
  Hostname string // hostname (without domain) of the above IP. If it begins with "ERROR!" it could not be determined
  Domain string   // domain of the above IP. If it begins with "ERROR!" it could not be determined
  Interface net.Interface // low level interface data
  HasPeers bool // true if DNS has SRV records for tcp/gosa-si (if Domain could not be determined, this will always be false)
}

// Information about each non-loopback interface that is UP.
var Interfaces = []InterfaceInfo{}

// index in Interfaces of the most appropriate interface to use. -1 if none could be determined
var BestInterface = -1

// Only log messages with level <= this number will be output.
// Note: The actual variable controlling the loglevel is util.LogLevel.
// This is just the value read from the config file.
var LogLevel int

// Maximum time permitted for a read or write transmission. If this time
// is exceeded, the transmission is aborted.
var Timeout = 5 * time.Minute

// If a peer is down for more than this time, its jobs are removed from the
// database and the connection to the peer is dropped. No reconnect will be
// attempted after this time, so unless the peer contacts us or go-susi is
// restarted (and the peer is listed in DNS or server.conf) there will be
// no further communication with this peer.
var MaxPeerDowntime = 7 * 24 * time.Hour

// When a request comes in from GOsa to modify or delete a foreign job,
// go-susi does not apply it directly to its own jobdb. Instead it
// forwards the request to the responsible siserver. Because of this,
// a gosa_query_jobdb done right after such a request will not reflect
// the changes. As that's exactly what GOsa does, the user experience
// is suboptimal because it will seem like the request had no effect.
// To compensate for this, gosa_query_jobdb delays for at most the
// duration specified in this variable. The delay only happens if
// a foreign job modification was forwarded shortly before the
// gosa_query_jobdb, so there is no delay during normal operation.
//
// GOsa note: GOsa has a very short timeout for gosa_query_jobdb
// requests (normally 5s). This duration needs to be shorter.
//
// Note: peer_connection:SyncIfNotGoSusi() bases its delay on
// this value to make sure that in the case a full sync is
// caused by a forwarded request, the full sync occurs before
// the delay of gosa_query_jobdb is finished. If this variable
// is changed peer_connection:SyncIfNotGoSusi() should be checked
// to make sure its derived wait time is still enough.
var GosaQueryJobdbMaxDelay = 4*time.Second

// If true, existing data in /var/lib/go-susi will be discarded.
var FreshDatabase = false

// true => add peer servers from DNS to serverdb.
var DNSLookup = true

// List of domains, each starting with a dot, that will be
// appended in turn to short names that DNS can't resolve.
var LookupDomains = []string{}

// URI for connecting to LDAP server.
var LDAPURI = "ldap://localhost:389"

// LDAP searches will be restricted to the subtree rooted at this DN.
var LDAPBase = "c=de"

// DN of the admin user for writing to LDAP.
var LDAPAdmin = "cn=clientmanager,ou=incoming,c=de"

// Password of the admin user for writing to LDAP.
var LDAPAdminPassword = "password"

// DN of the user for reading from LDAP. Empty string means anonymous.
var LDAPUser = ""

// Password of the user for reading from LDAP.
var LDAPUserPassword = ""

// Filter that is ANDed with all LDAP queries. Must be enclosed in parentheses if non-empty.
var UnitTagFilter = ""

// true if "--version" is passed on command line
var PrintVersion = false

// true if "--help" is passed on command line
var PrintHelp = false

// Parses args and sets config variables accordingly.
func ReadArgs(args []string) {
  LogLevel = 0
  for i := 0; i < len(args); i++ {
    arg := args[i]
  
    if arg == "-v" || arg == "-vv" || arg == "-vvv" || arg == "-vvvv" || 
       arg == "-vvvvv" || arg == "-vvvvvv" || arg == "-vvvvvvv" {
    
      LogLevel = len(arg) - 1
    
    } else if arg == "-f" {
    
      FreshDatabase = true
    
    } else if strings.HasPrefix(arg, "--test=") {
    
      testdir := arg[7:]
      LogFilePath = testdir + "/go-susi.log"
      ServerConfigPath = testdir + "/server.conf"
      JobDBPath = testdir + "/jobdb.xml"
      ServerDBPath = testdir + "/serverdb.xml"
      
    } else if arg == "-c" {
      i++
      if i >= len(args) {
        util.Log(0, "ERROR! ReadArgs: missing argument to -c")
      } else {
        ServerConfigPath = args[i]
      }
    } else if arg == "--help" {
    
      PrintHelp = true
      
    } else if arg == "--version" {      
      
      PrintVersion = true
      
    } else {
      util.Log(0, "ERROR! ReadArgs: Unknown command line switch: %v", arg)
    }
  }
}

// Parses the relevant configuration files and sets 
// the config variables accordingly.
func ReadConfig() {
  file, err := os.Open(ServerConfigPath)
  if err != nil {
    util.Log(0, "ERROR! ReadConfig: %v", err)
    return
  }
  defer file.Close()
  input := bufio.NewReader(file)
  
  conf := map[string]map[string]string{"":map[string]string{}}
  current_section := ""
  for {
    var line string
    line, err = input.ReadString('\n')
    if err != nil { break }
    
    line = strings.TrimSpace(line)
    if len(line) > 2 && line[0] == '[' && line[len(line)-1] == ']' {
      current_section = line
      if _, ok := conf[current_section]; !ok {
        conf[current_section] = map[string]string{}
      }
    }
    
    i := strings.Index(line, "=")
    if i >= 0 {
      key := strings.TrimSpace(line[0:i])
      value := strings.TrimSpace(line[i+1:])
      if key != "" {
        conf[current_section][key] = value
      }
    }
  }
  
  if err != io.EOF {
    util.Log(0, "ERROR! ReadConfig: %v", err)
    // Do not return. Try working with whatever we got out of the file.
  }
  
  for sectionname, section := range conf {
    if sectkey, ok := section["key"]; ok {
      ModuleKeys = append(ModuleKeys, sectkey)
      ModuleKey[sectionname] = sectkey
    }
  }
  
  if general, ok := conf["[general]"]; ok {
    if logfile, ok := general["log-file"]; ok {
      LogFilePath = logfile
    }
  }
  
  if serverpackages, ok := conf["[ServerPackages]"]; ok {
    if addresses, ok := serverpackages["address"]; ok && addresses != "" {
      PeerServers = append(PeerServers, strings.Fields(strings.Replace(addresses,","," ",-1))...)
    }
    if dnslookup, ok := serverpackages["dns-lookup"]; ok {
      dnslookup = strings.TrimSpace(dnslookup)
      if dnslookup != "false" && dnslookup != "true" {
        util.Log(0, "ERROR! ReadConfig: [ServerPackages]/dns-lookup must be \"true\" or \"false\", not \"%v\"", dnslookup)
      }
      DNSLookup = (dnslookup == "true")
    }
    if lookupdomains, ok := serverpackages["domains"]; ok {
      for _, dom := range strings.Fields(strings.Replace(lookupdomains,","," ",-1)) {
        if dom[0] != '.' { dom = "." + dom }
        LookupDomains = append(LookupDomains, dom)
      }
    }
  }
  
  if server, ok:= conf["[server]"]; ok {
    
    if port,ok := server["port"]; ok {
      port = strings.TrimSpace(port)
      i := strings.Index(ServerSourceAddress,":")
      ServerSourceAddress = ServerSourceAddress[:i+1] + port
      i = strings.Index(ServerListenAddress,":")
      ServerListenAddress = ServerListenAddress[:i+1] + port
    }
    
    if uri, ok := server["ldap-uri"]; ok { LDAPURI = uri }
    if base,ok := server["ldap-base"]; ok { LDAPBase = base }
    if admin,ok:= server["ldap-admin-dn"]; ok { LDAPAdmin = admin }
    if pw  ,ok := server["ldap-admin-password"]; ok { LDAPAdminPassword = pw }
    if user,ok := server["ldap-user-dn"]; ok { LDAPUser = user }
    if pw  ,ok := server["ldap-user-password"]; ok { LDAPUserPassword = pw }
  }
}

// Reads network parameters.
func ReadNetwork() {
  var err error
  
  var ifaces []net.Interface
  ifaces, err = net.Interfaces()
  if err != nil {
    util.Log(0, "ERROR! ReadNetwork: %v", err)
  } else
  {
    best_interface_weight := -1
    
    // find non-loopback interfaces that are up
    for _, iface := range ifaces {
      if iface.Flags & net.FlagLoopback != 0 { continue }
      if iface.Flags & net.FlagUp == 0 { continue }
      
      ifaceInfo := InterfaceInfo{}
      ifaceInfo.Interface = iface
      
      var addrs []net.Addr
      addrs, err = iface.Addrs()
      if err == nil {
        
        // find the first IP address for that interface
        for _, addr := range addrs {
          ip, _, err2 := net.ParseCIDR(addr.String())
          if err2 == nil && !ip.IsLoopback() {
            ifaceInfo.IP = ip.String()
            goto FoundIP
          }
        }
        err = fmt.Errorf("Could not determine IP for interface %v", iface.HardwareAddr.String())
      FoundIP:
      }
      
      if err != nil { 
        ifaceInfo.IP = fmt.Sprintf("ERROR! %v", err)
        ifaceInfo.Hostname = ifaceInfo.IP
        ifaceInfo.Domain = ifaceInfo.IP
      } else
      {
        var names []string
        names, err = net.LookupAddr(ifaceInfo.IP)
        //util.Log(2, "DEBUG! Names for %v: %v", ifaceInfo.IP, names)
        if err == nil {
          for _, name := range names {
            name = strings.Trim(name, ".")
            if name == "" { continue }
            
            // if we have no hostname yet, use the name from the address
            // if this includes a "." we'll chop off the domain in the if below
            if ifaceInfo.Hostname == "" { ifaceInfo.Hostname = name }
            
            i := strings.Index(name, ".")
            if i > 0 {
              ifaceInfo.Hostname = name[0:i]
              ifaceInfo.Domain = name[i+1:]
              goto DomainFound
            }
          }
          err = fmt.Errorf("Could not determine domain. Lookup of IP %v returned %v", ifaceInfo.IP, names)
        DomainFound:
        } 
        
        if err != nil {
          if ifaceInfo.Hostname == "" { ifaceInfo.Hostname = fmt.Sprintf("ERROR! %v", err) }
          ifaceInfo.Domain = fmt.Sprintf("ERROR! %v", err)
        }
      }
      
      if !strings.HasPrefix(ifaceInfo.Domain, "ERROR!") {
        var addrs []*net.SRV
        _, addrs, err := net.LookupSRV("gosa-si", "tcp", ifaceInfo.Domain)
        if err != nil {
          util.Log(0, "ERROR! LookupSRV(\"gosa-si\",\"tcp\",\"%v\"): %v", ifaceInfo.Domain, err) 
        } else 
        { 
          ifaceInfo.HasPeers = (len(addrs) > 0)
        }
      }
      
      Interfaces = append(Interfaces, ifaceInfo)
      
      weight := 0
      if !strings.HasPrefix(ifaceInfo.IP, "ERROR!") { weight += 1 }
      if !strings.HasPrefix(ifaceInfo.Hostname, "ERROR!") { weight += 2 }
      if !strings.HasPrefix(ifaceInfo.Domain, "ERROR!") { weight += 4 }
      if ifaceInfo.HasPeers { weight += 8 }
      
      if BestInterface < 0 || weight > best_interface_weight { 
        BestInterface = len(Interfaces) - 1 
        best_interface_weight = weight
      }
    }
  }
  
  // use os.Hostname as default in case we can't get a host name from an interface
  var hostname string
  hostname, err = os.Hostname()
  if err == nil { Hostname = hostname }

  if BestInterface >= 0 {
    MAC = Interfaces[BestInterface].Interface.HardwareAddr.String()
    if !strings.HasPrefix(Interfaces[BestInterface].Hostname, "ERROR!") {
      Hostname = Interfaces[BestInterface].Hostname
    }
    if !strings.HasPrefix(Interfaces[BestInterface].Domain, "ERROR!") {
      Domain = Interfaces[BestInterface].Domain
    }
    if !strings.HasPrefix(Interfaces[BestInterface].IP, "ERROR!") {
      IP = Interfaces[BestInterface].IP
    }
    ServerSourceAddress = IP + ServerListenAddress[strings.Index(ServerListenAddress,":"):]
  }
  
  util.Log(1, "INFO! Hostname: %v  Domain: %v  MAC: %v  Server: %v", Hostname, Domain, MAC, ServerSourceAddress)
}

