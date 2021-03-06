<?php

class filterLDAP {

  static function query($base, $scope, $filter, $attributes, $category, $objectStorage= array(""))
  {
    $config= session::global_get('config');
    $ldap= $config->get_ldap_link(TRUE);
    $flag= ($scope == "sub")?GL_SUBSEARCH:0;
    $result= filterLDAP::get_list($base, $filter, $attributes, $category, $objectStorage, $flag | GL_SIZELIMIT);
    return $result;
  }


  static function get_list($base, $filter, $attributes, $category, $objectStorage, $flags= GL_SUBSEARCH)
  {
    $ui= session::global_get('ui');
    $config= session::global_get('config');

    // Move to arrays for category and objectStorage
    if (!is_array($category)) {
      $category= array($category);
    }

    // Store in base - i.e. is a rdn value empty?
    $storeOnBase= count($objectStorage) == 1 && empty($objectStorage[0]);

    $default_method= ($storeOnBase && !($flags & GL_SUBSEARCH))?"ls":"search";

    // Detect weither empty tags shall be shown
    $include_empty_tags = ($flags & GL_FILTER_INCLUDE_EMPTY_TAG)
        ? TRUE
        : FALSE;

    // Initialize search bases
    $bases= array();
    
    // Get list of sub bases to search on
    if ($storeOnBase) {
      $bases[$base]= "";
    } else {
      foreach ($objectStorage as $oc) {

        // Handle empty storage locatios here, maybe get_ou() as returned an empty string.
        if(empty($oc)){
            $bases[$base] = "";
            continue;
        }

        $oc= preg_replace('/,$/', '', $oc);
        $tmp= explode(',', $oc);
        if (count($tmp) == 1) {
          preg_match('/([^=]+)=(.*)$/', $oc, $m);
          if ($flags & GL_SUBSEARCH) {
            $bases[$base][]= $m[1].":dn:=".$m[2];
          } else {
            $bases["$oc,$base"][]= $m[1].":dn:=".$m[2];
          }
        } else {
          // No, there's no \, in pre defined RDN values
          preg_match('/^([^,]+),(.*)$/', $oc, $matches);
          preg_match('/([^=]+)=(.*)$/', $matches[1], $m);
          if ($flags & GL_SUBSEARCH) {
            $bases[$base][]= $m[1].":dn:=".$m[2];
          } else {
            $bases[$matches[2].",$base"][]= $m[1].":dn:=".$m[2];
          }
        }
      }
    }

    // Get LDAP link
    $ldap= $config->get_ldap_link($flags & GL_SIZELIMIT);

    // Do search for every base
    $result= array();
    $limit_exceeded = FALSE;

    // Save initial selected base for comparison purposes
    $initial_base = $base;

    foreach($bases as $base => $dnFilters) {
      $method = $default_method;

      // Break if the size limit is exceeded
      if($limit_exceeded){
        return($result);
      }

      // Switch to new base and search
      if (is_array($dnFilters)){
        $dnFilter= "(|";
        foreach ($dnFilters as $df) {
          $dnFilter.= "($df)";
        }
        $dnFilter.= ")";
      } else {
        $dnFilter= "";
      }
      $ldap->cd($base);

      // In no-subsearch mode overwrite default method if
      // current base equals the initial base
      if ($base == $initial_base && !($flags & GL_SUBSEARCH)) {
          $method = "ls";
      }

      if ($method == "ls") {
          $ldap->ls("(&$filter$dnFilter)", $base, $attributes, TRUE,
            $include_empty_tags);
      } else {
          $ldap->search("(&$filter$dnFilter)", $attributes, TRUE,
            $include_empty_tags);
      }

      // Check for size limit exceeded messages for GUI feedback
      if (preg_match("/size limit/i", $ldap->get_error())){
        session::set('limit_exceeded', TRUE);
        $limit_exceeded = TRUE;
      } 

      /* Crawl through result entries and perform the migration to the
         result array */
      while($attrs = $ldap->fetch()) {
        $dn= $ldap->getDN();

        /* Convert dn into a printable format */
        if ($flags & GL_CONVERT){
          $attrs["dn"]= convert_department_dn($dn);
        } else {
          $attrs["dn"]= $dn;
        }

        /* Skip ACL checks if we are forced to skip those checks */
        if($flags & GL_NO_ACL_CHECK){
          $result[]= $attrs;
        }else{

          /* Sort in every value that fits the permissions */
          foreach ($category as $o){
            if((preg_match("/\//",$o) && preg_match("/r/",$ui->get_permissions($dn,$o))) ||
                (!preg_match("/\//",$o) && preg_match("/r/",$ui->get_category_permissions($dn, $o)))){
              $result[]= $attrs;
              break;
            }
          }
        }
      }

    }

    return $result;
  }


}

?>
