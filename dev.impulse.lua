-- Dev impulse.lua for testing

function impulse00 ()
    print("impulse00")
end

function newsurfcb (v_id)
    print("New Surface -- In Lua callback")
end

xk_register_cb(XKEV_NEW_SURFACE, "newsurfcb")
