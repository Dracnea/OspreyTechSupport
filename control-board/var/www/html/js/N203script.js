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
    document.getElementById("a_miner_setting").style.color = '#ffffff';
    document.getElementById("a_miner_setting").style.backgroundColor = '';
    document.getElementById("a_miner_setting_1").style.borderRight = 'none';
    document.getElementById("a_ip_setting").style.color = '#ffffff';
    document.getElementById("a_ip_setting").style.backgroundColor = '';
    document.getElementById("a_ip_setting_1").style.borderRight = 'none';   
    document.getElementById("a_upfirm").style.color = '#ffffff';
    document.getElementById("a_upfirm").style.backgroundColor = '';
    document.getElementById("a_upfirm_1").style.borderRight = 'none';      
    document.getElementById("a_dignostic").style.color = '#ffffff';
    document.getElementById("a_dignostic").style.backgroundColor = '';
    document.getElementById("a_dignostic_1").style.borderRight = 'none';      
    document.getElementById("a_password").style.color = '#ffffff';
    document.getElementById("a_password").style.backgroundColor = '';
    document.getElementById("a_password_1").style.borderRight = 'none';          
    document.getElementById("a_dash_board").style.color = '#ffffff';
    document.getElementById("a_dash_board").style.backgroundColor = '';
    document.getElementById("a_dash_board_1").style.borderRight = 'none';      
    document.getElementById("a_eth_miner").style.color = '#ffffff';
    document.getElementById("a_eth_miner").style.backgroundColor = '';
    document.getElementById("a_eth_miner_1").style.borderRight = 'none';       
    document.getElementById("miner_config").style.color = '#ffffff';
    document.getElementById("miner_config").style.backgroundColor = '';
    document.getElementById("miner_config_1").style.borderRight = 'none';            
    document.getElementById("a_streaming_log").style.color = '#ffffff';
    document.getElementById("a_streaming_log").style.backgroundColor = '';
    document.getElementById("a_streaming_log_1").style.borderRight = 'none';          
    document.getElementById("a_fan").style.color = '#ffffff';
    document.getElementById("a_fan").style.backgroundColor = '';
    document.getElementById("a_fan_1").style.borderRight = 'none';          
    document.getElementById("a_noti").style.color = '#ffffff';
    document.getElementById("a_noti").style.backgroundColor = '';
    document.getElementById("a_noti_1").style.borderRight = 'none';          
    document.getElementById("a_advance").style.color = '#ffffff';
    document.getElementById("a_advance").style.backgroundColor = '';
    document.getElementById("a_advance_1").style.borderRight = 'none';             
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
        document.getElementById("a_ip_setting").style.color = '#ffffff';
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
      //  document.getElementById("a_streaming_log").style.color = '#8C8787';
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
function click_submenu(index)
{   
    clear_color();
     // 
     // isClickMiner = 0;
     //  isClickSystem = 0;

    if (index == 1)
    {
        document.getElementById("a_miner_setting").style.color = '#ffffff';
        document.getElementById("a_miner_setting").style.backgroundColor = '#0065b2';
        document.getElementById("a_miner_setting_1").style.borderRight = '4px solid white';                
    }
    else if (index == 83)
    {
        document.getElementById("a_ip_setting").style.color = '#ffffff';
        document.getElementById("a_ip_setting").style.backgroundColor = '#0065b2';
        document.getElementById("a_ip_setting_1").style.borderRight = '4px solid white';        
    }
    else if (index == 3)
    {
        document.getElementById("a_upfirm").style.color = '#ffffff';
        document.getElementById("a_upfirm").style.backgroundColor = '#0065b2';
        document.getElementById("a_upfirm_1").style.borderRight = '4px solid white';        
        hiden_sub_menu() ; 
        isClickSystem = 0; 
        isClickMiner = 0;                         
    }
    else if (index == 84)
    {
        document.getElementById("a_password").style.color = '#ffffff';
        document.getElementById("a_password").style.backgroundColor = '#0065b2';
        document.getElementById("a_password_1").style.borderRight = '4px solid white';
                    
    }
    else if (index == 5)
    {
        document.getElementById("a_dash_board").style.color = '#ffffff';   
        document.getElementById("a_dash_board").style.backgroundColor = '#0065b2';
        document.getElementById("a_dash_board_1").style.borderRight = '4px solid white';
        hiden_sub_menu() ;
        isClickSystem = 0; 
        isClickMiner = 0;                 
    }
    else if (index == 61)
    {
        document.getElementById("miner_config").style.color = '#ffffff';
        document.getElementById("miner_config").style.backgroundColor = '#0065b2';
        document.getElementById("miner_config_1").style.borderRight = '4px solid white';
    }    
    else if (index == 6)
    {
        document.getElementById("a_eth_miner").style.color = '#ffffff'; 
        document.getElementById("a_eth_miner").style.backgroundColor = '#0065b2';
        document.getElementById("a_eth_miner_1").style.borderRight = '4px solid white';        
        hiden_sub_menu() ; 
        isClickSystem = 0; 
        show_hide_miner();                  
    }
    else if (index == 7)
    {
        document.getElementById("a_streaming_log").style.color = '#ffffff';
        document.getElementById("a_streaming_log").style.backgroundColor = '#0065b2';
        document.getElementById("a_streaming_log_1").style.borderRight = '4px solid white';        
    }

    else if (index == 81)
    {
        document.getElementById("a_fan").style.color = '#ffffff';
        document.getElementById("a_fan").style.backgroundColor = '#0065b2';
        document.getElementById("a_fan_1").style.borderRight = '4px solid white';        
        hiden_sub_menu() ;  
           isClickSystem = 0; 
        isClickMiner = 0;                         
    }
    else if (index == 82)
    {
        document.getElementById("a_noti").style.color = '#ffffff';
        document.getElementById("a_noti").style.backgroundColor = '#0065b2';
        document.getElementById("a_noti_1").style.borderRight = '4px solid white';        
    }
    else if (index == 8)
    {   
        hiden_sub_menu() ;
        show_hide_system();
        isClickMiner = 0;
        document.getElementById("a_advance").style.color = '#ffffff';
        document.getElementById("a_advance").style.backgroundColor = '#0065b2';
        document.getElementById("a_advance_1").style.borderRight = '4px solid white';        
    }
    else if (index == 9)
    {
        
        document.getElementById("a_dignostic").style.color = '#ffffff';
        document.getElementById("a_dignostic").style.backgroundColor = '#0065b2';
        document.getElementById("a_dignostic_1").style.borderRight = '4px solid white';        
        hiden_sub_menu() ; 
        isClickSystem = 0; 
        isClickMiner = 0;  
    }

    
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
