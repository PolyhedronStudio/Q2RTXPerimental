var fn = Fn.new{|argA|
    System.print( argA + " test" )
    return true
}

// Call it with Hello World.
fn.call( "Hello World" )