function show_mode(input)
{   if (input.value == 1)
    {
        //document.getElementById("div_auto").style.display = "block";
        document.getElementById("div_manual").style.display = "none";
    }
    else
    {
        //document.getElementById("div_auto").style.display = "none";
        document.getElementById("div_manual").style.display = "block";                    
    }
}

function show_menu()
{
    document.getElementById("Table_Menu").style.visibility="visible";
}

function clear_color()
{
    document.getElementById("a_miner_setting").style.color = '#38455e';
     document.getElementById("a_ip_setting").style.color = '#38455e';
    document.getElementById("a_upfirm").style.color = '#ffffff';
    document.getElementById("a_dignostic").style.color = '#ffffff';
    document.getElementById("a_password").style.color = '#38455e';
    document.getElementById("a_dash_board").style.color = '#ffffff';
    document.getElementById("a_eth_miner").style.color = '#ffffff';
    document.getElementById("a_streaming_log").style.color = '#38455e';
    document.getElementById("a_fan").style.color = '#ffffff';
    document.getElementById("a_noti").style.color = '#38455e';
    document.getElementById("a_advance").style.color = '#ffffff';                                  
}

function hiden_sub_menu() {
    document.getElementById("sub3").style.display = "none";
    document.getElementById("advancesup").style.display = "inline"
    document.getElementById("advancesdown").style.display = "none"

document.getElementById("id_log").style.display = "none";
document.getElementById("minerup").style.display = "inline"
document.getElementById("minerdown").style.display = "none"
}

function show_hide_system()
{
    if (isClickSystem == 0)
    {
        isClickSystem = 1;
        document.getElementById("sub3").style.display = "block";
        document.getElementById("a_ip_setting").style.color = '#F5A623';
        document.getElementById("advancesup").style.display = "none"
        document.getElementById("advancesdown").style.display = "inline"


    }
    else
    {   
        document.getElementById("sub3").style.display = "none";
            document.getElementById("advancesup").style.display = "inline"
        document.getElementById("advancesdown").style.display = "none"
        isClickSystem = 0;
    }
}


function show_hide_miner()
{
    if (isClickMiner == 0)
    {
        isClickMiner = 1;
        document.getElementById("id_log").style.display = "block";
      //  document.getElementById("a_streaming_log").style.color = '#F5A623';
        document.getElementById("minerup").style.display = "none"
        document.getElementById("minerdown").style.display = "inline"

    }
    else
    {   
        document.getElementById("id_log").style.display = "none";
                document.getElementById("minerup").style.display = "inline"
        document.getElementById("minerdown").style.display = "none"
        isClickMiner = 0;
    }
}


function click_submenu(index)
{   
    clear_color();
     // 
     // isClickMiner = 0;
     //  isClickSystem = 0;
    if (index == 1)
    {
        document.getElementById("a_miner_setting").style.color = '#F5A623';      
    }
    else if (index == 83)
    {
        document.getElementById("a_ip_setting").style.color = '#F5A623';
    }
    else if (index == 3)
    {
        document.getElementById("a_upfirm").style.color = '#F5A623';    
        hiden_sub_menu() ; 
        isClickSystem = 0; 
        isClickMiner = 0;                         
    }
    else if (index == 84)
    {
        document.getElementById("a_password").style.color = '#F5A623';                    
    }
    else if (index == 5)
    {
        document.getElementById("a_dash_board").style.color = '#F5A623';   
        hiden_sub_menu() ;
        isClickSystem = 0; 
        isClickMiner = 0;                 
    }
    else if (index == 6)
    {
        document.getElementById("a_eth_miner").style.color = '#F5A623'; 
        hiden_sub_menu() ; 
        isClickSystem = 0; 
        show_hide_miner();                  
    }
    else if (index == 7)
    {
        document.getElementById("a_streaming_log").style.color = '#F5A623';                    
    }

    else if (index == 81)
    {
        document.getElementById("a_fan").style.color = '#F5A623';
        hiden_sub_menu() ;  
           isClickSystem = 0; 
        isClickMiner = 0;                         
    }
    else if (index == 82)
    {
        document.getElementById("a_noti").style.color = '#F5A623';                    
    }
    else if (index == 8)
    {   
        hiden_sub_menu() ;
        show_hide_system();
        isClickMiner = 0;
        document.getElementById("a_advance").style.color = '#F5A623';                    
    }
    else if (index == 9)
    {
        
        document.getElementById("a_dignostic").style.color = '#F5A623';  
                hiden_sub_menu() ; 
        isClickSystem = 0; 
        isClickMiner = 0;  
    }

    
}

function    show_dashboard()
{
    
}

function show_config()
{
    document.getElementById("div_show_config").style.display = "block";
    document.getElementById("div_show_status").style.display = "none";
    document.getElementById("div_show_upfirmware").style.display = "none";
    document.getElementById("div_show_loadbitstream").style.display = "none";
}

function show_status()
{  
    document.getElementById("div_show_config").style.display = "none";
    document.getElementById("div_show_status").style.display = "block";
    document.getElementById("div_show_upfirmware").style.display = "none";
    document.getElementById("div_show_loadbitstream").style.display = "none";
}

function show_upfimrware()
{   
    document.getElementById("div_show_config").style.display = "none";
    document.getElementById("div_show_status").style.display = "none";
    document.getElementById("div_show_upfirmware").style.display = "block";
    document.getElementById("div_show_loadbitstream").style.display = "none";
}


function show_loadbit()
{
    
    document.getElementById("div_show_config").style.display = "none";
    document.getElementById("div_show_status").style.display = "none";
    document.getElementById("div_show_upfirmware").style.display = "none";
    document.getElementById("div_show_loadbitstream").style.display = "block";
}